extern crate libc;
use libc::c_char;
use std::ffi::{CString, CStr};

use std::env;

use std::time::Instant;

use std::sync::mpsc::{channel, Receiver, Sender};
use std::sync::{Arc, Mutex};
use std::thread;
use std::sync::atomic::{AtomicBool, Ordering};

use std::io::{BufReader, BufRead, Error, ErrorKind};
use std::fs::{File, OpenOptions};
use std::io::prelude::*;


//const BUFF_SIZE: usize = 1024; // we dont need this because channel uses vec
const BUFF_ENTRY_SIZE: usize = 1024;

const MAX_INFILES: usize = 10;

const MAX_REQUESTERS: usize = 5;
const MAX_RESOLVERS: usize = 10;

extern
{
    fn dnslookup(hostname: *const libc::c_char, firstIPstr: *const libc::c_char, maxSize: i32) -> i32;
}

// safe rust bindings for the c dnslookup function
fn dns_lookup(hostname: &str) -> Result<String, &str>
{
    let out_buf: [c_char; BUFF_ENTRY_SIZE] = [0; BUFF_ENTRY_SIZE];
    let buf_ptr = out_buf.as_ptr();

    let hostname_str = CString::new(hostname).unwrap();
    let c_hostname_str = hostname_str.as_ptr();

    let res = unsafe { dnslookup(c_hostname_str, buf_ptr, BUFF_ENTRY_SIZE as i32) };

    if res == 0
    {
        let c_str: &CStr = unsafe { CStr::from_ptr(buf_ptr) };
        let str_slice: &str = c_str.to_str().unwrap();
        Ok(str_slice.to_owned())
    }
    else
    {
        Err("UTIL_FAILURE")
    }
}

struct ConcBufReader{inner: Arc<Mutex<Option<BufReader<File>>>>}

struct ConcFileWriter{inner: Arc<Mutex<File>>}

impl ConcBufReader
{
    pub fn read_line(&self, buf: &mut String) -> std::io::Result<usize>
    {
        let mut a = self.inner.lock().unwrap();
        match &mut *a
        {
            Some(ref mut buf_reader) => buf_reader.read_line(buf),
            None => Err(Error::new(ErrorKind::ConnectionRefused, "File empty."))
        }
    }
}

impl ConcFileWriter
{
    pub fn writeln(&self, data: &str) -> std::io::Result<()>
    {
        let a = self.inner.lock().unwrap();
        writeln!(&*a, "{}", data)
    }
}

impl Clone for ConcFileWriter
{
    fn clone(&self) -> Self
    {
        ConcFileWriter{inner: self.inner.clone()}
    }
}

fn get_open_file(fd_list: &Vec<(ConcBufReader, AtomicBool)>) -> Option<usize>
{
    for i in 0..fd_list.len()
    {
        if fd_list[i].1.compare_and_swap(true, false, Ordering::Relaxed) // if true set to false and then
        {
            return Some(i);
        }
    }
    return None;
}

fn requester_loop(fd_list: Arc<Vec<(ConcBufReader, AtomicBool)>>, tx: Sender<String>, log_fd: ConcFileWriter)
{
    let mut working_file_op: Option<usize> = get_open_file(&fd_list);
    let mut files_served = 0;

    while let Some(working_file) = working_file_op
    {
        let mut buf = String::new();
        match fd_list[working_file].0.read_line(&mut buf)
        {
            Ok(len) if len > 0 => { let _ = tx.send(buf.trim().to_owned()); },
            _ => { working_file_op = get_open_file(&fd_list); files_served+=1; }
        }
        //println!("buf: {}", buf.trim());
        //std::thread::sleep_ms(100);
    }
    let res = format!("Thread Files served: {}", files_served);
    let _ = log_fd.writeln(&res);
    //println!("{}", res);
}

fn resolver_loop(rx: Arc<Mutex<Receiver<String>>>, log_fd: ConcFileWriter)
{
    loop
    {
        let data = {
            let inner_rx = rx.lock().unwrap();

            match inner_rx.recv()
            {
                Ok(data) => data,
                _ => break
            }
        };

        let res = match dns_lookup(&data)
        {
            Ok(dns_res) => format!("{}, {}", &data, &dns_res),
            Err(_) => format!("{}, ", &data)
        };

        // write to file
        let _ = log_fd.writeln(&res);
        //println!("{}", res);
        //drop(res);
    }
}

fn start_requester_resolver_loop<'a>(num_req: i32, num_res: i32, req_log_file: &str, res_log_file: &str, in_files: Vec<&'a str>)
{
    let (tx, rx) = channel::<String>(); // reader writer buffer
    let rx = Arc::new(Mutex::new(rx));

    // I would sort of like to use tokio or futures but for this project I am opting more for threads and channels

    let mut req_childs = Vec::new();
    let mut res_childs = Vec::new();

    let req_log_fd = OpenOptions::new().write(true).append(true).create(true).open(req_log_file).unwrap();
    let res_log_fd = OpenOptions::new().write(true).truncate(true).create(true).open(res_log_file).unwrap();

    let req_log_fd = ConcFileWriter{inner: Arc::new(Mutex::new(req_log_fd))};
    let res_log_fd = ConcFileWriter{inner: Arc::new(Mutex::new(res_log_fd))};

    let fd_list = Arc::new(in_files.iter().map(|file| 
    {
        let fd = OpenOptions::new().read(true).open(file).ok();  // we want a list of Options
        (ConcBufReader{inner: match fd
            {
                Some(fd) => Arc::new(Mutex::new(Some(BufReader::new(fd)))),
                None => Arc::new(Mutex::new(None)) // this means that the file is closed for reading
            }
        }, AtomicBool::new(true))
    }).collect::<Vec<(ConcBufReader, AtomicBool)>>()); // a little over on the arcs

    for _i in 0..num_req
    {
        let this_req_log_fd = req_log_fd.clone();
        let this_tx = tx.clone();
        let this_fd_list = fd_list.clone();
        req_childs.push(thread::spawn(move || requester_loop(this_fd_list, this_tx, this_req_log_fd)));
    }

    for _i in 0..num_res
    {
        let this_res_log_fd = res_log_fd.clone();
        let this_rx = rx.clone();
        res_childs.push(thread::spawn(move || resolver_loop(this_rx, this_res_log_fd)));
    }

    for child in req_childs
    {
        child.join().unwrap();
    }
    drop(tx); // The resolver exits when all tx clones are dropped

    for child in res_childs
    {
        child.join().unwrap();
    }

}

fn main()
{
    let args: Vec<String> = env::args().collect();

    let num_requesters: i32;
    let num_resolvers: i32;

    let requesters_log_file: &str;
    let resolvers_log_file: &str;
    let mut in_files: Vec<&str> = Vec::new();

    if args.len() > MAX_INFILES + 5
    {
        println!("Wrong number of inputs. Expected:");
        println!("multi-lookup <#requesters> <#resolvers> <#req_log> <#res_log> [<#infile>, ...]")
    }
    else if args.len() < 6
    {
        println!("Wrong number of inputs. Expected:");
        println!("multi-lookup <#requesters> <#resolvers> <#req_log> <#res_log> [<#infile>, ...]")
    }
    else
    {
        num_requesters = args[1].parse::<i32>().unwrap();
        num_resolvers = args[2].parse::<i32>().unwrap();
        requesters_log_file = &args[3];
        resolvers_log_file = &args[4];

        for i in 0..(args.len()-5)
        {
            in_files.push(&args[i+5])
        }

        if num_requesters as usize > MAX_REQUESTERS
        {
            println!("Too many requesters, max: {}", MAX_REQUESTERS);
        }
        else if num_resolvers as usize > MAX_RESOLVERS
        {
            println!("Too many resolvers, max: {}", MAX_RESOLVERS);
        }
        else
        {
            let now = Instant::now();

            start_requester_resolver_loop(num_requesters, num_resolvers, requesters_log_file, resolvers_log_file, in_files);

            let elapsed = now.elapsed();


            println!("Took {} micro secs ({} sec) to finish", elapsed.as_micros(), elapsed.as_micros() as f32 / 1000000.0);
        }
    }
}

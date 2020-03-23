extern crate libc;
use libc::c_char;
use std::ffi::{CString, CStr};

const BUFF_SIZE: usize = 1024;
const BUFF_ENTRY_SIZE: usize = 1024;

const MAX_INFILES: usize = 10;

const MAX_REQUESTERS: usize = 5;
const MAX_RESOLVERS: usize = 10;

//#[link(name = "util", kind = "static")]
extern
{
    fn dnslookup(hostname: *const libc::c_char, firstIPstr: *const libc::c_char, maxSize: i32) -> i32;
}

fn dns_lookup(hostname: &str) -> Result<String, &str>
{
    let out_buf: [c_char; BUFF_ENTRY_SIZE] = [0; BUFF_ENTRY_SIZE];
    let buf_ptr = out_buf.as_ptr();

    let hostname_str = CString::new(hostname).unwrap();
    let c_hostname_str = hostname_str.as_ptr();

    let res = unsafe { dnslookup(c_hostname_str, buf_ptr, BUFF_ENTRY_SIZE as i32) };

    match res
    {
        0 => 
        {
            let c_str: &CStr = unsafe { CStr::from_ptr(buf_ptr) };
            let str_slice: &str = c_str.to_str().unwrap();
            Ok(str_slice.to_owned())
        },
        _ => Err("UTIL_FAILURE")
    }
}

fn main() {

    let res0 = dns_lookup("google.com");
    let res1 = dns_lookup("sdjjdsaf.com");
    let res2 = dns_lookup("rust-lang.org");

    println!("res0: {:?}", res0);
    println!("res1: {:?}", res1);
    println!("res2: {:?}", res2);
}

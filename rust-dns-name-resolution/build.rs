extern crate cc;

fn main() {
    let c_file = "../PA3/util.c";
    let lib_name = "dnsutil";
    // Tell Cargo that if the given file changes, to rerun this build script.
    println!("cargorerun-if-changed={}", c_file);
    // Use the `cc` crate to build a C file and statically link it.
    cc::Build::new()
        .file(c_file)
        .compile(lib_name);
}
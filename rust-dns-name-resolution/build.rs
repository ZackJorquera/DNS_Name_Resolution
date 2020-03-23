extern crate cc;

use std::env;

fn main() {
    /*
    // run first: gcc -g -o libdnsutil.a -c src/util.c
    let project_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    let lib = "dnsutil";

    println!("cargo:rustc-link-search={}", project_dir); // the "-L" flag
    println!("cargo:rustc-link-lib={}",lib); // the "-l" flag
    */

    
    // Tell Cargo that if the given file changes, to rerun this build script.
    println!("cargo:rerun-if-changed=src/hello.c");
    // Use the `cc` crate to build a C file and statically link it.
    cc::Build::new()
        .file("src/util.c")
        .compile("dnsutil");
}
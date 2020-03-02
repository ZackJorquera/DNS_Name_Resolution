# Multi Lookup
stuff

## Use
To use this program you must first build it you can do that with `make`.

From there all you need todo is run the resulting `multi-lookup` binary.
```
./multi-lookup 
```
It will tell you what inputs it expects.

## How it works
This program uses threading in a producer-consumer model where requester threads read from the input files and then put the domain names into a shared buffer. From here, the resolvers take the domain names and process then to get the IPv4 addresses and put those into an output log file.
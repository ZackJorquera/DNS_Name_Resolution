# DNS_Name_Resolution
Just a school assignment on concurrent programming.

I also did the assignment in Rust because why not.

## Timing
This is based on the internet so it is not very accurate.

```
./multi-lookup <num_req> <num_res> test1 test2 input/names1.txt input/names2.txt input/names3.txt input/names4.txt input/names5.txt input/names1.txt input/names2.txt input/names3.txt input/names4.txt input/names5.txt
```

num_req | num_res | C time (sec) | Rust time (sec)
--- | --- | --- | ---
1 | 1 | 7.674794 | 8.18086
1 | 3 | 3.988849 | 3.898404
3 | 1 | 7.068168 | 7.652322
3 | 3 | 3.733510 | 3.708637
3 | 5 | 3.554077 | 3.449155
5 | 3 | 3.790088 | 3.772899
5 | 5 | 3.645053 | 3.667211
5 | 10 | 3.683226 | 3.481854

Don't trust this data too much, I just took the best of 3 runs.

I have no idea why the C times got slower at the end. I re-ran this a couple of times and was still seeing the same results. 
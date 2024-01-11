# Best Practices and Performance Optimizations



## Fast transmission of messages

(WinRT doesn't do pointers, passes by value/copy in most cases, including (sometimes) arrays)

(sending words - still smaller than a pointer for most messages)

(sending IMemoryBuffer)
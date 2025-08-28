# TOY REDIS STREAMS:

## How to run some things:

I would suggest using the codespace I have created because I'm nervous about running in different environments

If you need to clear before hand run
`make clean`

Otherwise run `make run_all_tests` or `make interface` for the interface.

Run the interface with `make run` or `./redis_stream`

In the interface you can run pretty similar commands in the official redis CLI. XADD, XREAD, etc.

Only the basic ones in the docs for streams are implemented and not full featured.

## Assumptions:

- So you need to operate on the assumption that this doesn't exactly implement everything that redis streams can do. Mainly only a some of the basic commands.
- The other assumption is that I wasn't going to need to implement server client networked communication for a local toy implementation like this so I didn't put any time into that. The interface itself is just a wrapper for the stream structure so you can actually use it's methods in a way that is similar to the redis commands but not fully complete. There are several things missing like some special characters you can use for ids, etc.
- I also used a map which is sorted as a "close" (as in it implements a red-black tree for indexing under the hood so things remain sorted) approximation for radix tree which redis uses. The only thing I have the indexed map for is for range queries which is pretty much any time you read data (I think?) or use trim. I don't have time to implement a thread safe radix tree.
- Also the paradigm for concurrency redis uses is event loop and not some basic threads.

## Mistakes and Possible Changes to be Made:

- I think I shouldn't have been as careless using long longs every where because eventually I had to use size() and cast between long longs and unsigned longs and there area lot of warnings but I don't think I should add anymore time fixing it. I knew this wasn't something I wasn't going to be paying attention to as much, I made sacrifices to finish and I don't think this was a good idea.
- I know I could have organized better and I just link the whole file instead of use headers which is not great, it's fine for a small toy project but this sort of thing was another thing I sacrificed.
- my first implementation of the interface was terrible, the new one is better but such a waist of time I just should have had the tests and that's it but I felt like it was important that one could play around with this and do things I didn't really test for.
- There are probably some security or bug related issues I didn't pay attention to as well.
- My interfaces is probably the worst way to interface with this as just regular input with no ease of use features normal CLIs have. This is garbage but I didn't want to overcomplicated it.
- And lastly my tests are terrible, it's been a while since I tested so barebones like this especially with threads that I don't know how effective this is, I tried. The simulated concurrent calls tests definitely don't replicate the many concurrent calls to a server you might have and so I don't reach resource constraints or issues with locks or anything. I only have one simple test that if my code didn't lock resources would deadlock, hopefully, but it doesn't so I guess we're good.

## Future Hardening/Productionizing Things You Should Probably Do Maybe:

- Switch to shared mutex and locking for concurrent reads instead of unilateral blocking with lock guard. Though I don't know how redis decided to do this when handling shared data.
- This is in-memory but also probably not the most efficient use of memory though I do think for Redis itself operating as a stream they don't delete anything anything that is user defined and you can enact some policies but it's not a cache. I'm not sure what they do exactly.
- I don't think I have any dangling pointers or memory leaks anywhere but I forgot about implementing anything for the constructor and destructor.
- So there's probably a much better way to implement an index sorted hash for this purpose and many people have done different things to solve this. Radix trees that Redis uses is one way but most I think use LSM trees but I literally just learned how they work in more detail the other day so I don't think I'll be implementing that in a day.
- Also if I implemented the networking part along with the client and the protocol I would have had to deal with concurrent I/O and Redis does this with event loops. I probably would have gone with an async I/O library like on from Boost or an event loop library like libenv for tried and true tested implementations. However I think for a little project like this that someone wants to learn from using your OS' own basic I/O multiplexer. There's universal POSIX compliant ones that are fine.

Maybe there's more but this is what I've got.

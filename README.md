# kopchik

> [!WARNING]
> THIS PROJECT IS WORK IN PROGRESS.
> **DO NOT** USE IT FOR ANY PRODUCTION DEVELOPMENT.
> THX.

## Usage

```sh
cmake -Bbuild .
make -Cbuild
./build/kopchik
```

## TODO:

- [x] parse request line
- [x] parse headers
- [ ] add handlers
- [ ] cli args
- [ ] dynamically allocate memory for request body
- [ ] switch to async sockets (epoll, kqueue) (libuv???)

# Interpreter
My own Bash interpreter in C writted for Operating Systems graduation.

### Scope

- [x] The ability to run commands from any number of arguments in the foreground mode (the interpreter waiting for the end of the process). Required to assess 3.0.
- [x] Redirect input and output streams to and from files. Required to assess 3.0.
- [ ] Operation comments. (wtf?)
- [ ] Running commands from any number of arguments in the background, along with notification of the completion of these processes (PID + status). Required to assess 3.5.
- [x] Service interactive and batch mode. Required to assess 3.5.
- [ ] Operation zombie processes. Required to assess 4.0.

### How to use

* Clone this repository
* run command `gcc bash.c -o bash -Wall -lreadline`, make sure you have `readline.h` library
* run `./bash` for interactive mode or `./bash <arguments>` for batch mode, eg. `./bash ls -laR`

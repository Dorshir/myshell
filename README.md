# Custom Shell Program

## Overview

This is a custom shell program written in C that supports various features such as command execution, redirection, background execution, built-in commands, variable handling, and more. The shell provides an interactive environment for running commands similar to standard Unix shells.

## Features

1. **Basic Command Execution**: Execute standard Unix commands with arguments.
2. **Redirection**:
   - Output redirection (`>`)
   - Append redirection (`>>`)
   - Error redirection (`2>`)
3. **Background Execution**: Run commands in the background using `&`.
4. **Built-in Commands**:
   - Change prompt (`prompt =`)
   - Print arguments (`echo`)
   - Change directory (`cd`)
   - Print the last command status (`echo $?`)
   - Exit the shell (`quit`)
   - Repeat the last command (`!!`)
5. **Signal Handling**: Custom message on `Control-C`.
6. **Pipes**: Chain multiple commands with `|`.
7. **Variable Handling**: Set and use custom variables.
8. **Flow Control**: Support for `if/else` statements.
9. **User Input**: Read user input and use it in commands.
10. **Command History**: Navigate through command history using arrow keys.

## Compilation

To compile the shell program, use the following command:
```sh
gcc -o myshell shell2.c

# Usage
To run the shell, execute:

'' ./myshell

# Examples
## Basic Commands and Redirection

hello: ls -l
hello: ls -l > file
hello: ls -l >> file
hello: ls no_such_file 2> error.log

# Built-in Commands
hello: prompt = myprompt
myprompt: echo abc xyz
myprompt: echo $?
myprompt: cd mydir
myprompt: pwd
myprompt: !!
myprompt: quit

# Background Execution
hello: sleep 5 &

# Using Variables
hello: $filename = "testfile.txt"
hello: echo "This is a test" > $filename
hello: cat $filename

# Read Command
hello: echo Enter your name:
hello: read username
hello: echo "Hello, $username!"

Notes
Use Control-C to test the custom signal handling.
Navigate through command history using the up and down arrow keys.
The shell does not support all features of a full Unix shell but provides a simplified environment for learning and testing.

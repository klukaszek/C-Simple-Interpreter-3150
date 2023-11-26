# CIS 3150 Assignment 4

Name: Kyle Lukaszek

ID: 1113798

Due: November 24th, 2023

## Compilation

To compile the program, run the following command:

```bash
make
```

This will create two executables, a4 and a4ng.

a4 is a graphical version of the interpreter that uses ncurses to display the output.
a4ng is a non-graphical version of the interpreter that uses printf to display the output.

## Usage

To run the program, run the following commands:

```bash
./a4 <input_file>
./a4ng <input_file>
```
The program reads a program from the file specified by the command line argument <input_file>. 

The program is then parsed and executed.

## Description

The language used is defined as follows:

```
int <var>           -declare a variable as an int with the name <var>

set <var> #         -set the variable <var> equal to #

begin               -start program, after all declarations, there is one begin command

end                 -last line of the program, there is one end command

add <var> #         
sub <var> #          
mult <var> #         

-these functions perform arithmetic operations using <var> and #,  for example: add <var> 3 means <var> = <var> + 3

div <var> #

print <var1> <var2> string     
- if the program is using the graphics mode it prints the string at row == <var1> and column == <var2> 
- if not in graphics mode it prints the contents of <var1>, then <var2>, then the string to stdout.
- the string cannot have any spaces within it

goto <lineNumber>        
- jump to the <lineNumber> and begin executing there
- you cannot jump before the begin line
- goto can jump anywhere between the begin and end commands, including both of those commands

if <var> <op> <var>      
- <op> can be eq, ne, gt, gte, lt, lte
- evaluates the expression
- if true then execute the next line
- if false then jump over next line
```

The program has been tested for memory leaks using valgrind and no memory leaks were found.

## Example

./a4 pgm1

```







       a==b
```

./a4ng pgm1

```
7 7 a==b
```

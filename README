Genius Calculator
=================

Although it's under heavy development, it's actually very usable.
I use it myself as my desktop calculator.

To make myself look important, I also made up an official looking name for
the programming language of Genius, it's called GEL, (Genius Extention
Language) :)

Features of Genius:

 * language is very close to real mathematical notation
 * arbitrary precision integers (2-36), multiple precision floats
 * uses rationals when possible
 * will calculate and show half calculated expressions if the calculation
   could not be completed
 * variables
 * user functions
 * variable and function references with C like syntax
 * anonymous functions
 * matrix support
 * complex numbers
 * more ...

Here's what doesn't work or isn't done yet: (somewhat of a TODO list)

- profile and make the code leaner and meaner
- optimize the engine a bit more
- fix modular arithmetic

How to use this thing: (this is just a quick description)

Just type in the expression and press enter to get a result.  The
expression is written in GEL (Genius Extention Language).  A simple
GEL expression looks just like a math expression. So you don't need
to learn GEL to use the calculator. GEL is used when you want to
define and use functions, and variables and do other cool things.

GEL is designed so that you can quickly get your result with the minimal
typing.

****************************************************************************
What follows is a simple description of GEL:

A program in GEL is basically an expression which evaluates to a number,
since there exist only numbers.

Previous result:

The "Ans" variable can be used to get the result of the last expression

so if you did some calculation and just wanted to add 389 to the result,
you'd do "Ans+389"

(you can also use the lowercase "ans" as well)

Functions:

For variables use the = operator, that operator sets the variable and
returns the number you set, so you can do something like "a=b=5", just
like in C. Variables are really functions with no argument

There are a number of built in functions (currently "e" "pi" "sin" "cos"
"tan", and more)

To type functions in:

function_name(argument1, argument2, argument3)
function_name()

to type in variables:

variable_name

(of course the number of arguments is different for each function)

NOTE: if you don't type the () in, the function will not be evaluated
but it will be returned as a "function node", unless of course if it's a
normal variable in which case it returns the value of the variable, so this
fact is used 
1) for variables
2) for passing functions to functions (explained below)

NOTE2: built in functions with 0 usually act like variables

Current built in functions (NOT COMPLETE AND OUT OF DATE):

name:		# of arguments:		description:

warranty	0			prints warranty and license info
exit		0			exits genius
quit		0			exits genius
help		0			displays a list of functions with
					short descriptions
sethelp		2 (string,string)	set a description for the above list,
					(to be used inside library files)
set_float_prec	1 (integer)		set the floating point precision
get_float_prec	0			get the current float_prec
set_max_digits	1 (integer)		set the maximum digits in a result
get_max_digits	0 			get the current max_digits
set_results_as_floats 1 (bool)		sets if the results should be always
					printed as floats
get_results_as_floats 0			returns if the results should be always
					printed as floats
set_scientific_notation 1 (bool)	sets if floats should be in scientific
					notation
get_scientific_notation 0		return if floats should be in scientific
					notation
set_full_expressions	1 (bool)	sets if we should print out full
					expressions for non-numeric return
					values (longer then a line)
get_full_expressions	0		return the full_expressions value
set_max_errors	1 (integer)		sets the maximum number of errors
					to return on one evaluation
get_max_errors	0			gets the maximum number of errors
					to return on one evaluation
error		1 (string)		prints an error to the error channel
print		1 (string)		prints a string onto the standard
					output
printn		1 (string)		prints a string onto the standard
					output without trailing carrige
					return
display		2 (string,value)	prints a string, a colon and then the
					value

sin		1 (value)		calculates the sin function
cos		1 (value)		calculates the cos function
sinh		1 (value)		calculates the hyperbolic sin function
cosh		1 (value)		calculates the hyperbolic cos function
tan		1 (value)		calculates the tan function
atan		1 (value)		calculates the inverse tan function
pi		0 			the number pi
e		0 			the number e (same as exp(1), but
					answer is cached)
sqrt		1 (value)		calculates the sqare root
exp		1 (value)		calculates the e^value (exponential
					function)
ln		1 (value)		calculates the natural logarithm
gcd		2 (integer,integer)	the greatest common divisor function
lcm		2 (integer,integer)	the least common multiple function
jacobi		2 (integer,integer)	obtain the jacobi symbol
legendre	2 (integer,integer)	obtain the legendre symbol
perfect_square	1 (integer)		return 1 if argument is a perfect
					square
max		2 (value,value)		returns the larger of the two values
min		2 (value,value)		returns the smaller of the two values
prime		1 (integer)		returns the n'th prime for primes up
					to n==100000
round		1 (value)		rounds the value
floor		1 (value)		greatest integer <= value
ceil		1 (value)		least integer >= value
trunc		1 (value)		truncate the number to an integer
float		1 (value)		make the number into a floating
					point number, no matter what type it
					was
Re		1 (value)		get the real part of a complex number
Im		1 (value)		get the imaginary part of a
					complex number
I		1 (integer)		make an identity matrix of the size n
rows		1 (matrix)		get the number of rows in a matrix
columns		1 (matrix)		get the number of columns in a matrix
det		1 (matrix)		calculate the determinant of a matrix
ref		1 (matrix)		reduce the matrix to row-echelon form
					using the gaussian elimination method
rref		1 (matrix)		reduce the matrix to reduced
					row-echelon form, this is usefull for
					solving systems of linear equations
set_size	3 (matrix,integer,integer)
					return a matrix which is truncated or
					extended according to the size given
					as rows, columns
is_value_only	1 (matrix)		are all the nodes in the matrix values

is_null		1 (anything)		returns true if the argument is a
					null (empty result not a 0)
is_value	1 (anything)		returns true if the argument is a
					scalar value
is_string	1 (anything)		returns true if the argument is a
					string
is_matrix	1 (anything)		returns true if the argument is a
					matrix
is_function	1 (anything)		returns true if the argument is a
					function (anonymous)
is_function_ref	1 (anything)		returns true if the argument is a
					reference variable to a function
is_complex	1 (anything)		returns true if the argument is a
					value and a complex number
is_real		1 (anything)		returns true if the argument is a
					value and a real number (note that
					this is not exactly the opposite of
					is_complex)
is_integer	1 (anything)		returns true if the argument is a
					non complex integer value
is_rational	1 (anything)		returns true if the argument is a
					non complex rational value (an
					integer is rational)
is_float	1 (anything)		returns true if the argument is a
					non complex floating point value

is_poly		1 (vector)		does this vector look like a
					polynomial (horizontal with value
					nodes only)
trimpoly	1 (vector)		trim the 0's off of the polynomial
					high powers
addpoly		2 (vector,vector)	add two polynomials
subpoly		2 (vector,vector)	subtract two polynomials
mulpoly		2 (vector,vector)	multiply two polynomials
derpoly		1 (vector)		returns the first derivative of
					a polynomial
der2poly	1 (vector)		returns the second derivative of
					a polynomial
polytostring	2 (vector,string)	convert the polynomial to a string
					which is a humal (and GEL) readable
					expression, the string argument is
					what is to be used for the variable
					name (such as "x")
polytofunc	1 (vector)		convert the polynomial into a function 
					which you can then assign to an
					identifier for use


Functions in the standard library (lib.gel) (OUT OF DATE):

name:		# of arguments:		description:

abs		1 (value or matrix)	absolute value (see also |x| operator)
sum		3 (from,to,function)	calculates the sum of the function
					which should take one argument
					which is an integer stepped from
					"from" to "to
prod		3 (from,to,function)	calculates the product of the
					function which should take one
					argument which is an integer
					stepped from "from" to "to
infsum		3 (function,start,inc)	try to calculate an infinite sum, to
					the point where the new terms
					calculated by function, don't make
					any difference on the sum, the index
					is passed as a single argument to the
					function
infsum2		4 (function,arg,start,inc)
					like the above, but pass arg as the
					first argument to function and the
					index as the second
nPr		2 (integer,integer)	calculate permutations
nCr		2 (integer,integer)	calculate combinations
pascal		1 (integer)		get the pascal's triangle as a matrix
					up to the n'th row (one more row then
					the argument)
fib		1 (integer)		returns n'th fibbonachi number
catalan		1 (integer)		returns the n'th catalan number
minimize	1 (func,x,incr)		"minimize" the function: start
					evaluating at x, by increments of
					incr and return the first x where
					func is 0
trace		1 (matrix)		calculate the trace of the matrix
adj		1 (matrix)		return the adjoint matrix
delrowcol	1 (matrix,row,col)	delete the row and column indicated
					from a matrix and return the rest
convol		2 (vector,vector)	takes two horizontal vectors and
					calculates convolution
convol_vec	2 (vector,vector)	as above, but returns a vector of
					the individual terms
matsum		1 (matrix)		sums up all the terms of the matrix
matsum_sq	1 (matrix)		sums up all the squares of terms of
					the matrix
matprod		1 (matrix)		multiplies all the terms of the matrix
diagonal	1 (vector)		make a diagonal matrix from a
					horizontal vector
swaprow		3 (matrix,row1,row2)	swap two rows of a matrix
sortv		1 (vector)		sort the elements in a horizontal vector
reversev	1 (vector)		reverse the elements in a horizontal vector
rad2deg		1 (value)		convert radians to degrees
deg2rad		1 (value)		convert degrees to radians
asinh		1 (value)		hyperbolic inverse sin function
acosh		1 (value)		hyperbolic inverse cos function
cot		1 (value)		cotangent function
coth		1 (value)		hyperbolic cotangent function
acot		1 (value)		inverse cotangent function
acoth		1 (value)		hyperbolic inverse cotangent function
tanh		1 (value)		hyperbolic tangent function
atanh		1 (value)		hyperbolic inverse tangent function
csc		1 (value)		cosecant function
csch		1 (value)		hyperbolic cosecant function
acsc		1 (value)		inverse cosecant function
acsch		1 (value)		hyperbolic inverse cosecant function
sec		1 (value)		secant function
sech		1 (value)		hyperbolic secant function
asec		1 (value)		inverse secant function
asech		1 (value)		hyperbolic inverse secant function
sign		1 (value)		return the sign of the value (-1,0,1)
log		2 (value,base)		log of the value to the base b
log10		1 (value)		log of the value to the base 10
log2		1 (value)		log of the value to the base 2
conj		1 (value)		conjugate of a complex value

string		1 (anything)		convert to a string

rowsum		1 (matrix)		sum up the elements in rows and return
					a vertical vector with the results
rowsum_sq	1 (matrix)		sum up the squares of elements in rows
					and return a vertical vector with the
					results
rowmedian	1 (matrix)		calculate the median of each row and
					return a vertical vector with results
rowaverage	1 (matrix)		calculate the average of each row and
					return a vertical vector with results
rowstdev	1 (matrix)		calculate the standard deviateion of
					each row and return a vertical vector
					with results
rowstdevp	1 (matrix)		calculate the population standard
					deviation of each row and return a
					vertical vector with results
median		1 (matrix)		calculate the median of the entire
					matrix
average		1 (matrix)		calculate the average of the entire
					matrix
stdev		1 (matrix)		calculate the standard deviation of
					the entire matrix
stdevp		1 (matrix)		calculate the population standard
					deviation of the entire matrix

apply_over_matrix1
		1 (matrix,function)	apply function to every element in
					the matrix, mostly used inside the
					library functions
apply_over_matrix2
		1 (matrix or value,matrix or value,function)
					apply function to two matrixes or
					a matrix and a scalar value


To define a function do:


function <identifier>(<comma separated argument names>) = <function body>

(you could also assign the anonymous function syntax to an identifier as in:
 <identifier> = (`() = <function body>)

NOTE: that's a backquote and signifies an anonymous function, by setting
it to a variable name you effectively define a function

for example:

function addup(a,b,c) = a+b+c

then "addup(1,1,1)" yields 3

Variable argument lists:

If you include "..." after the last argument name, then genius will allow
any number of arguments to be passed in place of that argument.  If no arguments
were passed then that argument will be set to null.  Else it will be a horizontal
vector with all the arguments.  For example:

function f(a,b...) = b

Then "f(1,2,3)" yields "[2,3]", while "f(1)" yields a null

Absoulte value:

You can make an absolute value of something by putting the |'s around it.
Example:

|a-b|

Separator:

Finally there is the ';' operator, which is a way to separate expressions,
such a combined expression will return whatever is the result of the last
one, so

3 ; 5

yeilds 5

This will require some parenthesizing to make it unambiguous sometimes,
especially if the ; is not the top most primitive. This slightly differs
from other programming languages where the ; is a terminator of statements,
whereas in GEL it's actually a binary operator. If you are familiar with
pascal this should be second nature. However genius can let you pretend
it's a terminator somewhat, if a ";" is found at the end of a parenthesis 
or a block, genius will itself append a null node to it as if you would
have written ";.". This is usefull in case you don't want to return a
value from say a loop, or if you handle the return differently. Note that
it will slow down the code if it's executed too often as there is one
more operator involved.

The GEL operators:

a;b		separator, just evaluates both but returns only b
a=b		assignment operator asigns b to a (a must be a valid lvalue)
|a|		absolute value
a^b		exponentiation
a.^b		element by element exponentiation
a+b		addition
a-b		subtraction
a*b		multiplication
a.*b		element by element multiplication
a/b		division
a./b		element by element division
a\b		back division
a.\b		element by element back division
a%b		the mod operator
a.%b		element by element the mod operator
a mod b		mod evaluation operator (a evaluated mod b)
a!		factorial operator
a!!		double factorial operator
a==b		equality operator (returns 1 or 0)
a!=b		inequality operator (returns 1 or 0)
a<>b		alternative inequality operator (returns 1 or 0)
a<=b		inequality operator (returns 1 or 0)
a>=b		inequality operator (returns 1 or 0)
a<=>b		comparison operator (returns -1, 0 or 1)
a and b		logical and
a or b		logical or
a xor b		logical xor
not a		logical not
-a		negation operator
&a		variable referencing (to pass a reference to something)
*a		variable dereferencing (to access a referenced varible)
a'		matrix conjugate transpose
a.'		matrix transpose
a@(b,c)		get element of a matrix
a@(b,)		get row of a matrix
a@(,c)		get column of a matrix
a..b		specify a row, column region
a@(b)		get an element from a matrix treating it as a vector

NOTE: the @() operator for matrixes is the only place you can use the ..
operator. With it you can specify a range of values instead of just
one. So that a@(2..4,6) is the rows 2,3,4 of the column 6. Or a@(,1..2)
will get you the first two columns of a matrix. You can also assign to
the @() operator, as long as the right value is a matrix that matches the
region in size, or if it is any other type of value. The exception to the
above rule is the a@(b) operator which will only take a single value,
it tries to treat the matrix as a vector, if the matrix is not a vector,
it will traverse the matrix row-wise.

NOTE: the comparison operators (except for the <=> operator which behaves
normally), are not strictly binary operators, they can in fact be grouped
in the normal mathematical way, e.g.: (1<x<=y<5) is a legal boolean
expression and means just what it should, that is (1<x and x<=y and y<5)

Lvalues:

Valid lvalues are

a		identifier
*a		dereference of an identifier
a@(<region>)	a region of a matrix (where the region is specified normally
		as with the regular @() operator)

Examples:

a=4
*tmp = 89
a@(4..8,3)=[1,2,3,4,5]'

There are also a number of constructs:

Conditionals:

if <expression1> then <expression2> [else <expression3>]

If else is omitted, then if the expression1 yeilds 0, NULL is returned.

Examples:

if(a==5)then(a=a-1)
if b<a then b=a
if c>0 then c=c-1 else c=0
a = ( if b>0 then b else 1 )

Loops:

while <expression1> do <expression2>
until <expression1> do <expression2>
do <expression2> while <expression1>
do <expression2> until <expression1>

These are similiar to other languages, they return the result of
the last iteration or NULL if no iteration was done

For loops:

for <identifier> = <from> to <to> do <body>
for <identifier> = <from> to <to> by <increment> do <body>

Loop with identifier being set to all values from <from> to <to>, optionally
using an increment other then 1. These are faster, nicer and more compact
then the normal loops such as above, but less flexible. The identifier must
be an identifier and can't be a dereference. The value of identifier is the
last value of identifier, or <from> if body was never evaluated. The variable
is guaranteed to be initialized after a loop, so you can safely use it.
Also the <from> <to> and <increment> must be non complex values. The <to>
is not guaranteed to be hit, but will never be overshot, for example the
following prints out odd numbers from 1 to 19

for i = 1 to 20 by 2 do print(i)

Foreach loops:

for <identifier> in <matrix> do <body>

For each element, going row by row from left to right do the body. To
print numbers 1,2,3 and 4 in this order you could do:

for n in [1,2:3,4] do print(n)

If you wish to run through the rows and columns of a matrix, you can use
the RowsOf and ColumnsOf functions which return a vector of the rows or
columns of the matrix.  So

for n in RowsOf ([1,2:3,4]) do print(n)

will print out [1,2] and then [3,4].

Sums and Products

sum <identifier> = <from> to <to> do <body>
sum <identifier> = <from> to <to> by <increment> do <body>
sum <identifier> in <matrix> do <body>
prod <identifier> = <from> to <to> do <body>
prod <identifier> = <from> to <to> by <increment> do <body>
prod <identifier> in <matrix> do <body>

If you substitute 'for' with 'sum' or 'prod', then you will get a sum or
a product instead of a for loop.  Instead of returning the last value,
these will return the sum or the product of the values respectively.

And now the comparison operators:

==,>=,<=,!=,<>,<,> return 1 for TRUE, 0 for FALSE

!= and <> are the same thing and mean "is not equal to". Make sure
you use == for equality however, as = will have the same outcomes as
it does in C.

<=> returns -1 if left side is smaller, 0 if both sides are equal, 1
    if left side is larger

To build up logical expressions use the words "not","and","or","xor"

"not" and "and" are special beasts as they evaluate their arguemnts one by
one, so the usual trick for conditional evaluation works here as well.
(E.g. "1 or a=1" will not set a=1 since the first argument was true)

You can also use break and continue, in the same manner as they are used
in C. Such as in (bn are booleans s is just some statement):

while(b1) do (
	if(b2) break
	else if(b3) continue;
	s1
)

Null:

Null is a special value, if it is returned, nothing is printed on
screen, no operations can be done on it. It is also usefull if you want
no output from a command. Null can be achieved as an expression when you
type . or nothing

Example:

    x=5;.
    x=5;

Returning:

Sometimes it's not good to return the last thing calculated, you may for
example want to return from a middle of a function. This is what the return
keyword is for, it takes one argument which is the return value

Example:

    function f(x) = (
	    y=1;
	    while(1) do (
		    if(x>50) then return y;
		    y=y+1;
		    x=x+1
	    )
    )

References: 

GEL contains references with a C like syntax. & references a variable
and * dereferences a variable, both can only be applied to an identifier
so **a is not legal in GEL

Example:

    a=1;
    b=&a;
    *b=2;

    now a contains 2

    function f(x) = x+1;
    t=&f;
    *t(3)

    gives us 4

Anonymous functions:

It is possible to say use a function in another function yet you don't know
what the function will be, you use an anonymous function. Anonymous function
is declared as:

function(<comma separated argument names>) = <function body>

or shorthand:

`(<comma separated argument names>) = <function body>

Example:

    function f(a,b) = a(b)+1;
    f(`(x) = x*x,2)

    will return 5 (2*2+1)

You can also just pass the function name as well:

    function f(a,b) = a(b)+1;
    function b(x) = x*x;
    f(b,2)


Matrix support:

To enter matrixes use one of the following two syntaxes. You can either
enter the matrix separating values by commas and rows by semicolons, or
separating values by tabs and rows by returns, or any combination of the
two. So to enter a 3x3 matrix of numbers 1-9 you could do

[1,2,3;4,5,6;7,8,9]

or

[1	2	3
 4	5	6
 7	8	9]

or

[1,2,3
 4,5,6
 7,8,9]

Do not use both ';' and return at once on the same line though. You can
however use tabs and commas together, as long as you use at most 1 comma
to separate values. To enter tabs inside the command line version, you have
to do M-Tab (usually Alt-Tab), or however else you have your .inputrc set
up (since it uses readline)

You can also use the matrix expansion functionality to enter matricies.
For example you can do:
a = [	1	2	3
	4	5	6
	7	8	9]
b = [	a	10
	11	12]

and you should get

[1	2	3	10
 4	5	6	10
 7	8	9	10
 11	11	11	12]

similiarly you can build matricies out of vectors and other stuff like that.

Another thing is that non-specified spots are initialized to 0, so

[1	2	3
 4	5
 6]

will end up being

[1	2	3
 4	5	0
 6	0	0]

NOTE: be careful about using whitespace and returns for expressions inside
the '[',']' brackets, they have a slightly different meaning and could mess
you up.

Transpose operator:

You can transpose a matrix by using the ' operator, example:

[1,2,3]*[4,5,6]'

We transpose the second vector to make matrix multiplication possible.


Modular evaluation (doesn't currently work):

Sometimes when working with large numbers, it might be faster if results
are modded after each calculation. So there is a modular evaluation operator
ever since 0.4.5. To use it you just add "mod <integer>" after the
expression.

Example:

    2^(5!) * 3^(6!) mod 5


Strings:

You can enter strings into gel and store them as values inside variables
and pass them to functions. (And do just about anything that values can
do). You can also concatenate the strings with something else (anything),
with the plus operator. So say:

a=2+3;"The result is: "+a

Will create a string "The result is: 5". You can also use C-like escape
sequences \n,\t,\b,\a,\r, to get a \ or " into the string you can qoute it
with a \. Example:

"Slash: \\ Quotes: \" Tabs: \t1\t2\t3"

will make a string:
Slash: \ Quotes: " Tabs: 	1	2	3

You can use the library function string to convert anything to a string.
Example:

string(22)

will return "22".

Strings can also be compared with ==, != and <=> operators


Error handeling:

If you detect an error in your function, you can bail out of it. You can
either fail to compute the function which is for normal errors, such as
wrong types of arguments, just add the empty statement "bailout". If something
went really wrong and you want to completely kill the current computation,
you can use "exception".

Look at lib.gel for some examples.

Polynomials:

Genius can do some things on polynomials. Polynomials in genius are just
horizontal vectors with value only nodes. The power of the term is the
position in the vector, with the first position being 0. So, [1,2,3]
translates to a polynomial of "1 + 2*x + 3*x^2".

You can add, subtract and multiply polynomials using the addpoly,
subpoly and mulpoly functions and you can print out a human readable
string using the polytostring function. This function takes the polynomial
vector as the first argument and a string to use as the variable as the
second argument and returns a string. You can also get a funcion
representation of the polynomial so that you can evaluate it. This is
done by the polytofunc function, which returns an anonymous function which
you can assign to something.

f = polytofunc([0,1,1])
f(2)

Look at the function table for the rest of polynomial functions.

Loading external programs:

Sometimes you have a larger program that you wrote into a file and want
to read in that file, you have two options, you can keep the functions
you use most inside a ~/.geniusinit file. Or if you want to load up a
file in a middle of a session (or from within another file), you can
type "load <list of filenames>" at the prompt. This has to be done
on the top level and not inside any function or whatnot, and it cannot
be part of any expression. It also has a slightly different syntax then the
rest of genius, more similiar to a shell. You can enter the file in
quotes. If you use the '' quotes, you will get exactly the string
that you typed, if you use the "" quotes, special characters will be
unescaped as they are for strings. Example:

load program1.gel program2.gel
load "Weird File Name With SPACES.gel"

There are also cd, pwd and ls commands built in


Calculator parameters:

There are several parameters that one can set that control the behaviour
of the calculator. You can either use functions with the prefixes get_ or
set_. The available parameters are:

float_prec		The floating point precision
max_digits		The maximum digits in a result
results_as_floats	If the results should be always printed as floats
scientific_notation	If floats should be in scientific notation
full_expressions	Boolean, should we print out full expressions
			for non-numeric return values (longer then a line)
max_errors		The maximum number of errors to return on one
			evaluation
mixed_fractions		If fractions should be printed as mixed fractions
		        such as "1 1/3" rather then "4/3"
integer_output_base	The base that will be used to output integers
output_style		A string, can be "normal", "latex" or "troff"
			and it will effect how matrices (and perhaps other
			stuff) is printed, useful for pasting into documents

You can use the parameters just like a variable, and you can put the name
on the left side of an equals sign. However, the parameters are not treated
as variables. They are translated into the get_ and set_ functions just
before execution. So you have to only use it to get the current value or
on the left side of the equals sign.


Standard startup procedure:

First the program looks for the installed library file (the compiled
version lib.cgel) in the installed directory, then it looks into the
current directory, and then it tries to load an uncompiled file called
~/.geniusinit

If you ever change the lib.gel in it's installed place, you'll have to
first compile it with "genius --compile lib.gel > lib.cgel"


EXAMPLE PROGRAM in GEL:
    a user factorial function (there already is one built in so
    this function is useless)

    function f(x) = if x<=1 then 1 else (f(x-1)*x)

    or with indentation it becomes

    function f(x) = (
	    if x<=1 then
		    1
	    else
		    (f(x-1)*x)
    )

    this is a direct port of the factorial function from the bc manpage :)
    it seems similiar to bc syntax, but different in that in gel, the
    last expression is the one that is returned. It can be done with with
    returns as:

    function f(x) = (
	    if (x <= 1) then return (1);
	    return (f(x-1) * x)
    )

    which is almost verbatim bc code, except for then and the function
    definition (although there is a compatibility bc like define keyword
    which will likely disappear in the forseeable future)

    Here's a smaller, nicer, iterative version:

    function f(x) = prod k=1 to x do k

    Here's a larger example, this basically redefines the internal ref
    function to calculate the same thing, but written in GEL:

    #calculate the row-echelon form of a matrix
    function ref(m) = (
	    if(not is_matrix(m) or not is_value_only(m)) then
		    (error("ref: argument not a value only matrix");bailout);
	    s=min(rows(m),columns(m));
	    i=1;
	    d=1;
	    while(d<=s and i<=columns(m)) do (

		    # This just makes the anchor element non-zero if at
		    # all possible
		    if(m@(d,i)==0) then (
			    j=d+1;
			    while(j<=rows(m)) do (
				    if(m@(j,i)==0) then (j=j+1;continue);
				    a=m@(j,);
				    m@(j,)=m@(d,);
				    m@(d,)=a;
				    j=j+1;
				    break
			    )
		    );
		    if(m@(d,i)==0) then (i=i+1;continue);
    
		    # Here comes the actual zeroing of all but the anchor
		    # element rows
		    j=d+1;
		    while(j<=rows(m)) do (
			    if(m@(j,i)!=0) then (
				    m@(j,)=m@(j,)-(m@(j,i)/m@(d,i))*m@(d,)
			    );
			    j=j+1
		    );
		    m@(d,) = m@(d,) * (1/m@(d,i));
		    d=d+1;
		    i=i+1
	    );
	    m
    )




****************************************************************************

Requirements:
	- lex (tested under flex)
	- yacc (tested under bison -y)
	- gmp (tested with 2.0.2)
	- glib
	- gtk+ (only for the GUI version)
	- gnome libs (only for the GUI version)
All except gmp seem to be pretty much standard or Linux systems, and even
on most other platforms. (except gtk and gnome libs, but since this is
distributed with gnome ...)

It's under GPL so read COPYING

George <jirka@5z.com>

-----------------------------------------------------------------------------
OLD history, read ChangeLog for new changes:

what's new since pre-alpha-1:
	- support for all the primitives
	- some scientific functions
	- uses gnomeapp
	- variables
	- user defined functions
	- and a lot more ... read the ChangeLog!

what's new since unusable-3:
	- version change :)
	- can read arbitrary ints (base#number#, e.g. 7#442#)
	- again some very slight tinkering with the gui
	- exponentials using rationals are now calculated via Newton's
	  method (e.g. 3^(33/12) )

what's new since unusable-2:
	- integer exponantiation, meaning you can do <whatever>^<integer>
	- the display box now stretches out to make use of free space in
	  the window
	- characters from the numpad now properly insert themselves
	- a bit of code cleanup
	- automaic conversion of floats to integers can be turned off since
	  it can make less precise numbers look precise
	- you can also turn off exact answers and get answers to a certain
	  precision (10 digits), or get the whole enchilada (sometimes
	  the listbox will crash for very large integers)

what's new since unusable-1:
	- floats are now formatted and output ok.
	- options frame for swiching between notations
	- negative numbers are handled by lexer so that they work in
	  postfix and prefix, this broke parser for infix, so turn off
	  negative functions for infix and use the unary minus operator
	  instead (same result)
	- factorial added (only for ints :)
	- it will now convert to int whenever possible even inside
	  calculations
	- parser.c renamed to calc.c since it didn't makes sense
	- GUI changed a bit
	- negation function is now functional it is ~ now ... but -
	  can also be used in infix, but not in postfix or prefix
	  since that would be ambiguous though you can still write
	  a negative number in any notation
	- division now works, it will make a rational number from integers
	  but will do a division for floats or rationals (rationals make
	  precise divisions)
	- it can now display unevaluated parts of equation when an error
	  occurs or a part just can't be evaluated (such as division by
	  zero)
	- results are now put on the display list in the GUI.

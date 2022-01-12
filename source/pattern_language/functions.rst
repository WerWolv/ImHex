Functions
=========

Functions are reusable pieces of code that can do calculations. Pretty much like functions in any other programming language.

Parameter types need to be specified explicitly, return type is automatically deduced.

.. code-block:: hexpat

    fn min(s32 a, s32 b) {
        if (a > b)
            return b;
        else
            return a;
    };

    std::print(min(100, 200)); // 100

Variables
^^^^^^^^^

Variables can be declared in a similar fashion as outside of functions but they will be put onto the function's stack instead of being highlighted.


.. code-block:: hexpat

    fn get_value() {
        u32 value;
        u8 x = 1234;

        value = x * 2;

        return value;
    };



Control statements
^^^^^^^^^^^^^^^^^^

If-Else-Statements
------------------

If, Else-If and Else statements work the same as in most other C-like languages.
When the condition inside a ``if`` head evaluates to true, the code in its body is executed.
If it evaluates to false, the optional ``else`` block is executed.

Curly braces are optional and only required if more than one statement is present in the body.

.. code-block:: hexpat

    if (x > 5) {
        // Execute when x is greater than 5
    } else if (x == 2) {
        // Execute only when x is equals to 2
    } else {
        // Execute otherwise
    }


While-Loops
-----------

While loops work similarly to if statements. As long as the condition in the head evaluates to true, the body will continuously be executed.

.. code-block:: hexpat

    while (check()) {
        // Keeps on executing as long as the check() function returns true
    }


For-Statement :version:`1.12.0`
--------------------------------

For loops are another kind of loop similar to the while loop. Its head consists of three blocks separated by commas.
The first block is a variable declaration which will only be available inside the current for loop.
The second block is a condition that will continuously be checked. The body is executed as long as this condition evaluates to true.
The third block is a variable assignment which will be executed after all statements in the body have run.

.. code-block:: hexpat

    // Declare a variable called i available only inside the for
    for (u8 i = 0, i < 10, i = i + 1) {
        // Keeps on executing as long as i is less than 10

        // At the end, increment i by 1
    }

Loop control flow statements :version:`1.13.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Inside of loops, the ``break`` and ``continue`` keyword may be used to to control the execution flow inside the loop.

When a ``break`` statement is reached, the loop is terminated immediately and code flow continues after the loop.
When a ``continue`` statement is reached, the current iteration is terminated immediately and code flow continues at the start of the loop again, checking the condition again.

Return statements
^^^^^^^^^^^^^^^^^

In order to return a value from a function, the ``return`` keyword is used.

The return type of the function will automatically be determined by the value returned.

.. code-block:: hexpat

    fn get_value() {
        return 1234;
    };

    std::print("{}", get_value()); // 1234
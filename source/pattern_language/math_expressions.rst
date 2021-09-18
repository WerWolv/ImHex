Mathematical Expressions
========================

Numeric Operators
^^^^^^^^^

.. table::
    :align: left

    ================= ===================================
    Operator          Description
    ================= ===================================
    ``a + b``         Addition
    ``a - b``         Subtraction
    ``a * b``         Multiplication
    ``a / b``         Division
    ``a % b``         Modulus
    ``a >> b``        Bitshift right
    ``a << b``        Bitshift left
    ``~a``            Bitwise NOT
    ``a & b``         Bitwise AND
    ``a | b``         Bitwise OR
    ``a ^ b``         Bitwise XOR
    ``a == b``        Equality comparison
    ``a != b``        Inequality comparison
    ``a > b``         Greater-than comparison
    ``a < b``         Less-than comparison
    ``a >= b``        Greater-than-or-equals comparison
    ``a <= b``        Less-than-or-equals comparison
    ``!a``            Boolean NOT
    ``a && b``        Boolean AND
    ``a || b``        Boolean OR
    ``a ^^ b``        Boolean XOR
    ``a ? b : c``     Ternary
    ``(a)``           Parenthesis
    ``function(a)``   Function call
    ================= ===================================

``a``, ``b`` and ``c`` can be any numeric literal or another expression.


Type Operators
^^^^^^^^^^^^^^

Type Operators are operators that work on types. They can only be used on a variable, not on a mathematical expression.

.. table::
    :align: left

    ================= ===================================
    Operator          Description
    ================= ===================================
    ``addressof(a)``  Base address of variable
    ``sizeof(a)``     Size of variable
    ================= ===================================

``a`` can be a variable, either by naming it directly or finding it through member access


String Operators
^^^^^^^^^^^^^^^^

String operators are any operators acting on strings directly.

.. table::
    :align: left

    ================= ===================================
    Operator          Description
    ================= ===================================
    ``a + b``         String concatination
    ``str * number``  String repetition
    ``a == b``        Lexical equality 
    ``a != b``        Lexical inequality
    ``a > b``         Lexical greater-than
    ``a >= b``        Lexical greater-than-or-equals
    ``a < b``         Lexical less-than
    ``a <= b``        Lexical less than-or-equals
    ================= ===================================

Member Access
^^^^^^^^^^^^^

Member access is the act of accessing members inside a struct, union or bitfield or referencing the index of an array to
access its value.

Below the simplest operations are shown, however they may be concatinated and extended indefinitely as suitable.

.. table::
    :align: left

    ================= =====================================================================================
    Operation         Access type
    ================= =====================================================================================
    ``structVar.var`` Accessing a variable inside a struct, union or bitfield
    ``arrayVar[x]``   Accessing a variable inside an array
    ``parent.var``    Accessing a variable inside the parent struct or union of the current struct or union
    ``this``          Refering to the current pattern. Can only be used inside of a struct or union
    ================= =====================================================================================


``$`` Dollar Operator
^^^^^^^^^^^^^^^^^^^^^

The Dollar Operator is a special operator which expands to the current offset within the current pattern.

.. code-block:: hexpat
    
    #pragma base_address 0x00

    std::print($); // 0
    u32 x @ 0x00;
    std::print($); // 4


Casting Operator
^^^^^^^^^^^^^^^^

The cast operator changes the type of an expression into another.

.. code-block:: hexpat
    
    fn test(float x) {
        return 1 + u32(x);
    }

    test(3.14159); // 4
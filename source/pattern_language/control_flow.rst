Control flow and conditional parsing
====================================

The ImHex pattern language offers multiple ways to modify the behaviour of the parser on the go based on the values already parsed, allowing you to parse complex formats with relative ease.

Conditionals
^^^^^^^^^^^^

In the pattern language, not all structs need to be the same size or have the same layout. It's possible to
change what variables are getting declared based on the value of other variables.

.. code-block:: hexpat

    enum Type : u8 {
        A = 0x54,
        B = 0xA0,
        C = 0x0B
    };

    struct PacketA {
        float x;
    };

    struct PacketB {
        u8 y;
    };

    struct PacketC {
        char z[];
    };

    struct Packet {
        Type type;
        
        if (type == Type::A) {
            PacketA packet;
        }
        else if (type == Type::B) {
            PacketB packet;
        }
        else if (type == Type::C)
            PacketC packet;
    };

    Packet packet[3] @ 0xF0;

This code looks at the first byte of each ``Packet`` to determine its type so it can decode the body of the packet accordingly using the correct types defined in ``PacketA``, ``PacketB`` and ``PacketC``.

.. image:: assets/conditionals/hex.png
  :width: 100%
  :alt: Conditional Highlighing

.. image:: assets/conditionals/data.png
  :width: 100%
  :alt: Conditional Decoding


Array control flow :version:`1.13.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The most basic form of conditional parsing are array control flow statements, ``break`` and ``continue``. These allow you to stop the parsing of the array or skip elements based on conditions in the currently parsed item instance.

Break :version:`1.13.0`
-----------------------

When a break is reached, the current array creation process is terminated. 
This means, the array keeps all entries that have already been parsed, including the one that's being currently processed, but won't expand further, even if the requested number of entries hasn't been reached yet.

.. code-block:: hexpat

  struct Test {
    u32 x;

    if (x == 0x11223344)
      break;
  };

  // This array requests 1000 entries but stops growing as soon as it hits a u32 with the value 0x11223344
  // causing it to have a size less than 1000
  Test tests[1000] @ 0x00;


Continue :version:`1.13.0`
--------------------------

When a continue is reached, the currently evaluated array entry gets evaluated to find next array entry offset but then gets discarded. 
This can be used to conditionally exclude certain array entries from the list that are either invalid or shouldn't be displayed in the pattern data list
while still scanning the entire range the array would span.

This can for instance be used in combination with :doc:`in/out variables </pattern_language/in_out>` to easily filter array items.

.. code-block:: hexpat

  struct Test {
    u32 value;

    if (value == 0x11223344)
      continue;
  };

  // This array requests 1000 entries but skips all entries where x has the value 0x11223344
  // causing it to have a size less than 1000
  Test tests[1000] @ 0x00;


Expressions
^^^^^^^^^^^

Operators
---------

.. table::
    :align: left

    ================= ==================================================
    Operator          Description
    ================= ==================================================
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
    ``function(a)``   :doc:`Function </pattern_language/functions>` call
    ================= ==================================================

``a``, ``b`` and ``c`` can be any numeric literal or another expression.


Type Operators
--------------

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
----------------

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
-------------

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
---------------------

The Dollar Operator is a special operator which expands to the current offset within the current pattern.

.. code-block:: hexpat
    
    #pragma base_address 0x00

    std::print($); // 0
    u32 x @ 0x00;
    std::print($); // 4


It's also possible to assign a value to the dollar operator to change the current cursor position.

.. code-block:: hexpat

    $ += 0x100;


Casting Operator
----------------

The cast operator changes the type of an expression into another.

.. code-block:: hexpat
    
    fn test(float x) {
        return 1 + u32(x);
    }

    test(3.14159); // 4

Data Types
==========

Types are the fundamental entity defining how a certain region of memory should be interpreted, formatted and displayed.

Built-in Types
^^^^^^^^^^^^^^

The simplest available types are the built-in standard types:

======== ==========
Unsigned Integers
-------------------
Name     Size   
======== ==========
``u8``   1 Byte
``u16``  2 Bytes
``u32``  4 Bytes
``u64``  8 Bytes
``u128`` 16 Bytes
======== ==========

======== ==========
Signed Integers
-------------------
Name     Size   
======== ==========
``s8``   1 Byte
``s16``  2 Bytes
``s32``  4 Bytes
``s64``  8 Bytes
``s128`` 16 Bytes
======== ==========

========== ==========
Floating Point
---------------------
Name       Size   
========== ==========
``float``  4 Bytes
``double`` 8 Bytes
========== ==========

========== ==========
Special
---------------------
Name       Size   
========== ==========
``char``   1 Bytes
``char16`` 2 Bytes
``bool``   1 Byte
========== ==========

* Unsigned integer types are displayed as a number ranging from ``0`` to ``((1 << bits) - 1)``.
* Signed integer types are displayed in decimal as a number in Two's complement ranging from ``-(1 << (bits - 1))`` to ``(1 << (bits - 1)) - 1``
* Floating point types are displayed according to ``IEEE 754``
* ``char`` is displayed as a ``ASCII`` character.
* ``char16`` is displayed as a ``UTF-16`` character.
* ``bool`` is displayed as ``false`` for the value zero, ``true`` for the value one and ``true*`` for any other value

Endianess
^^^^^^^^^

By default all built-in types are interpreted in native endianess. 
Meaning if ImHex is running on a little endian machine, all types will be treated as little endian. On a big endian machine they will be treated as big endian.

However it's possible to override this default on a global, per-type or per-variable basis.
Simply prefix any type with the ``le`` for little endian or ``be`` for big endian keyword:

.. code-block:: hexpat

    le u32 myUnsigned;  // Little endian 32 bit unsigned integer
    be double myDouble; // Big endian 64 bit double precision floating point
    s8 myInteger;       // Native endian 8 bit signed integer


Arrays
^^^^^^

To create a list of consequitive variables of the same type, an array can be defined.

.. code-block:: hexpat

    u32 mySizedArray[100];
    u8 myUnsizedArray[];
    float myLoopArray[while(someCondition)];

``mySizedArray`` defines the simplest type of array. It has exactly 100 u32 entries.
``myUnsizedArray`` defines a unsized array which keeps on growing until it hits an entry that evaluates to ``0x00``.
``myLoopArray`` defines an array that keeps on growing and for each entry checks if ``someCondition`` still evaluates to ``true``. As soon as it doesn't anymore, the array stops growing.

Arrays can be used inside structs and unions or be placed using the placement syntax.

Strings
-------

``char`` and ``char16`` types act differently when they are used in an array.
Instead of displaying as an array of characters, they are displayed as a String instead; terminated by a null byte in the following example.

.. code-block:: hexpat

    char myCString[];
    char16 myUTF16String[];


Using declarations
^^^^^^^^^^^^^^^^^^

Using declarations are useful to give existing types a new name and optionally add extra specifiers to them.
The following code creates a new type called ``Offset`` which is a big endian 32 bit unsigned integer. It can be used in place of any other type now.

.. code-block:: hexpat

    using Offset = be u32;
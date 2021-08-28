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

* Unsigned integer types are displayed as a number ranging from ``0`` to ``((1 << bits) - 1)``.
* Signed integer types are displayed in decimal as a number in Two's complement ranging from ``-(1 << (bits - 1))`` to ``(1 << (bits - 1)) - 1``
* Floating point types are displayed according to ``IEEE 754``


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


Custom Data Types
^^^^^^^^^^^^^^^^^
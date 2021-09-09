Enums
=====

Enums are datatypes that consist of a set of named constants of a specific size. 

They are particularly useful for associating meaning to a value found in memory.
Defining an enum works similar to other C-like languages. The first entry in the enum will be associated the value ``0x00`` and each following one will count up from there.
If an entry has an explicit value assigned to it, every entry following it will continue counting from there.

.. code-block:: hexpat

  enum StorageType : u16 {
    Plain,    // 0x00
    Compressed = 0x10,
    Encrypted // 0x11
  };

The type following the colon after the enum name declares the enum's underlying type and can be any built-in datatype.
This type only affects the enum's size.

.. image:: assets/enums/data.png
  :width: 100%
  :alt: Enums Decoding
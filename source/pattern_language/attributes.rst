Attributes
==========

Attributes are special directives that can add extra configuration individual variables and types.

.. code-block:: hexpat

  struct RGBA8 {
    u8 red   [[color("FF0000")]];
    u8 green [[color("00FF00")]];
    u8 blue  [[color("0000FF")]];
  } [[static]];

.. note::

  | Attributes can either have no arguments: ``[[attribute_name]]``
  | Or a single string argument: ``[[attribute_name("attribute_value")]]``

It's also possible to apply multiple attributes to the same variable or type: ``[[attribute1, attribute2]]``

------------------------

Variable Attributes
^^^^^^^^^^^^^^^^^^^

``[[color("RRGGBB")]]``
-----------------------

Changes the color the variable is being highlighted in the hex editor.

``[[name("new_name")]]``
------------------------

Changes the display name of the variable in the pattern data view without affecting the rest of the program.

``[[comment("text")]]``
-----------------------

Adds a comment to the current variable that is displayed when hovering over it in the pattern data view.

``[[format("formatter_function_name")]]``
-----------------------------------------

Overrides the default display value formatter with a custom function. 
The function requires a single argument representing the value to be formatted (e.g ``u32`` if this attribute was applied to a variable of type ``u32``) and return a string which will be displayed in place of the default value.

``[[hidden]]``
--------------

Prevents a variable from being shown in the pattern data view but still be usable from the rest of the program.

``[[inline]]``
--------------

Can only be applied to Arrays and Struct-like types. Visually inlines all members of this variable into the parent scope. 
Useful to flatten the displayed tree structure and avoid unnecessary indentation while keeping the pattern structured. 


Type Attributes
^^^^^^^^^^^^^^^

``[[static]]``
--------------

| ImHex by default optimizes arrays of built-in types so they don't use up as much memory and are evaluated quicker.
| This same optimization can be applied to custom data types when they are marked with this attribute to tell ImHex the size and layout of this type will always be the same.
| **However** if the layout of the type this is applied to is not static, highlighing and decoding of the data will be wrong and only use the layout of the first array entry.
Namespaces
==========

Namespaces provide a way to encapsulate and group multiple similar types together. They also allow for multiple types in different namespaces to be named the same without interfering with each other.

.. code-block:: hexpat

    namespace abc {

        struct Type {
            u32 x;
        };
        
    }

    abc::Type type1 @ 0x100;

    using Type = abc::Type;
    Type type2 @ 0x200;

To access a type within a namespace, the scope resolution operator ``::`` is used. In the example above, to access the type ``Type`` inside the namespace ``abc``, ``abc::Type`` is used.

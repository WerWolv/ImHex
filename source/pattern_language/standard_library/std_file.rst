``File I/O Library`` :version:`1.12.0`
======================================

.. code-block:: hexpat

    #include <std/file.pat>

| This namespace contains various pointer helper functions
|

.. warning::

    These functions are considered dangerous and require the user to confirm that they really want to run this pattern.

------------------------

Types
-----

``Handle``
^^^^^^^^^^

**A type representing a File Handle**

``Mode``

**File Opening Mode**
- ``Read``: Open a file in read-only mode. Throws an error if the file doesn't exist.
- ``Write``: Open a file in Read/Write mode. Throws an error if the file doesn't exist.
- ``Create``: Opens a new file in Read/Write mode. If the file doesn't exist, it will be created. If it does exist, it will be cleared.

Functions
---------

``std::file::open(str path, std::file::Mode mode) -> std::file::Handle`` :version:`1.12.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Opens a file on disk and returns a handle to it**
Similar to C's ``fopen``

.. important::

    Every file opened **MUST** be closed before the program ends or the handle goes out of scope.
    Otherwise File handles will be leaked and the file cannot be opened again until ImHex is restarted.

.. table::
    :align: left

    =========== ================================================================================
    Parameter   Description
    =========== ================================================================================
    ``path``    Path to a file on disk. Either absolute or relative to current working directory
    ``mode``    A value of ``std::file::Mode`` specifying how to open the file
    ``return``  Handle to the opened file
    =========== ================================================================================

------------------------

``std::file::close(std::file::Handle handle)`` :version:`1.12.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Closes a file previously opened with** ``std::file::open()``
Similar to C's ``fclose``

.. table::
    :align: left

    =========== =========================================================
    Parameter   Description
    =========== =========================================================
    ``handle``  Handle to the opened file
    =========== =========================================================

------------------------

``std::file::read(std::file::Handle handle, u128 size) -> str`` :version:`1.12.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Reads data as a string from a file**
Similar to C's ``fread``

.. table::
    :align: left

    =========== =========================================================================
    Parameter   Description
    =========== =========================================================================
    ``handle``  Handle to the opened file
    ``size``    Number of characters to read. If value is ``0``, the entire file is read.
    ``return``  String containing read characters
    =========== =========================================================================

------------------------

``std::file::write(std::file::Handle handle, str data)`` :version:`1.12.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Writes data in form of a string to a file**
Similar to C's ``fwrite``

.. table::
    :align: left

    =========== =========================================================================
    Parameter   Description
    =========== =========================================================================
    ``handle``  Handle to the opened file
    ``data``    Data to write to current offset in file
    =========== =========================================================================

------------------------

``std::file::seek(std::file::Handle handle, u128 offset)`` :version:`1.12.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Seeks to a specific offset in the file**
Similar to C's ``fseek``

.. table::
    :align: left

    =========== =========================================================================
    Parameter   Description
    =========== =========================================================================
    ``handle``  Handle to the opened file
    ``offset``  Offset in the file to seek to
    =========== =========================================================================

------------------------

``std::file::size(std::file::Handle handle) -> u128`` :version:`1.12.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Queries the length of a file**

.. table::
    :align: left

    =========== =========================================================================
    Parameter   Description
    =========== =========================================================================
    ``handle``  Handle to the opened file
    ``return``  Size of file in bytes
    =========== =========================================================================

------------------------

``std::file::resize(std::file::Handle handle, u128 size)`` :version:`1.12.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Resizes a file**
Similar to C's ``ftruncate``

.. table::
    :align: left

    =========== =================================================================================================
    Parameter   Description
    =========== =================================================================================================
    ``handle``  Handle to the opened file
    ``size``    New size of file. If ``size`` is smaller than the current size, excess characters will be deleted
    =========== =================================================================================================

------------------------

``std::file::flush(std::file::Handle handle)`` :version:`1.12.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Flushes all currently pending disk operations**
Similar to C's ``fflush``

.. table::
    :align: left

    =========== =================================================================================================
    Parameter   Description
    =========== =================================================================================================
    ``handle``  Handle to the opened file
    =========== =================================================================================================

------------------------

``std::file::remove(std::file::Handle handle)`` :version:`1.12.0`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Deletes a file from disk**

.. table::
    :align: left

    =========== =================================================================================================
    Parameter   Description
    =========== =================================================================================================
    ``handle``  Handle to the opened file
    =========== =================================================================================================
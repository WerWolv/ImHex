# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindPackageHandleStandardArgs
-----------------------------

This module provides functions intended to be used in :ref:`Find Modules`
implementing :command:`find_package(<PackageName>)` calls.

.. command:: find_package_handle_standard_args

  This command handles the ``REQUIRED``, ``QUIET`` and version-related
  arguments of :command:`find_package`.  It also sets the
  ``<PackageName>_FOUND`` variable.  The package is considered found if all
  variables listed contain valid results, e.g. valid filepaths.

  There are two signatures:

  .. code-block:: cmake

    find_package_handle_standard_args(<PackageName>
      (DEFAULT_MSG|<custom-failure-message>)
      <required-var>...
      )

    find_package_handle_standard_args(<PackageName>
      [FOUND_VAR <result-var>]
      [REQUIRED_VARS <required-var>...]
      [VERSION_VAR <version-var>]
      [HANDLE_VERSION_RANGE]
      [HANDLE_COMPONENTS]
      [CONFIG_MODE]
      [NAME_MISMATCHED]
      [REASON_FAILURE_MESSAGE <reason-failure-message>]
      [FAIL_MESSAGE <custom-failure-message>]
      )

  The ``<PackageName>_FOUND`` variable will be set to ``TRUE`` if all
  the variables ``<required-var>...`` are valid and any optional
  constraints are satisfied, and ``FALSE`` otherwise.  A success or
  failure message may be displayed based on the results and on
  whether the ``REQUIRED`` and/or ``QUIET`` option was given to
  the :command:`find_package` call.

  The options are:

  ``(DEFAULT_MSG|<custom-failure-message>)``
    In the simple signature this specifies the failure message.
    Use ``DEFAULT_MSG`` to ask for a default message to be computed
    (recommended).  Not valid in the full signature.

  ``FOUND_VAR <result-var>``
    .. deprecated:: 3.3

    Specifies either ``<PackageName>_FOUND`` or
    ``<PACKAGENAME>_FOUND`` as the result variable.  This exists only
    for compatibility with older versions of CMake and is now ignored.
    Result variables of both names are always set for compatibility.

  ``REQUIRED_VARS <required-var>...``
    Specify the variables which are required for this package.
    These may be named in the generated failure message asking the
    user to set the missing variable values.  Therefore these should
    typically be cache entries such as ``FOO_LIBRARY`` and not output
    variables like ``FOO_LIBRARIES``.

    .. versionchanged:: 3.18
      If ``HANDLE_COMPONENTS`` is specified, this option can be omitted.

  ``VERSION_VAR <version-var>``
    Specify the name of a variable that holds the version of the package
    that has been found.  This version will be checked against the
    (potentially) specified required version given to the
    :command:`find_package` call, including its ``EXACT`` option.
    The default messages include information about the required
    version and the version which has been actually found, both
    if the version is ok or not.

  ``HANDLE_VERSION_RANGE``
    .. versionadded:: 3.19

    Enable handling of a version range, if one is specified. Without this
    option, a developer warning will be displayed if a version range is
    specified.

  ``HANDLE_COMPONENTS``
    Enable handling of package components.  In this case, the command
    will report which components have been found and which are missing,
    and the ``<PackageName>_FOUND`` variable will be set to ``FALSE``
    if any of the required components (i.e. not the ones listed after
    the ``OPTIONAL_COMPONENTS`` option of :command:`find_package`) are
    missing.

  ``CONFIG_MODE``
    Specify that the calling find module is a wrapper around a
    call to ``find_package(<PackageName> NO_MODULE)``.  This implies
    a ``VERSION_VAR`` value of ``<PackageName>_VERSION``.  The command
    will automatically check whether the package configuration file
    was found.

  ``REASON_FAILURE_MESSAGE <reason-failure-message>``
    .. versionadded:: 3.16

    Specify a custom message of the reason for the failure which will be
    appended to the default generated message.

  ``FAIL_MESSAGE <custom-failure-message>``
    Specify a custom failure message instead of using the default
    generated message.  Not recommended.

  ``NAME_MISMATCHED``
    .. versionadded:: 3.17

    Indicate that the ``<PackageName>`` does not match
    ``${CMAKE_FIND_PACKAGE_NAME}``. This is usually a mistake and raises a
    warning, but it may be intentional for usage of the command for components
    of a larger package.

Example for the simple signature:

.. code-block:: cmake

  find_package_handle_standard_args(LibXml2 DEFAULT_MSG
    LIBXML2_LIBRARY LIBXML2_INCLUDE_DIR)

The ``LibXml2`` package is considered to be found if both
``LIBXML2_LIBRARY`` and ``LIBXML2_INCLUDE_DIR`` are valid.
Then also ``LibXml2_FOUND`` is set to ``TRUE``.  If it is not found
and ``REQUIRED`` was used, it fails with a
:command:`message(FATAL_ERROR)`, independent whether ``QUIET`` was
used or not.  If it is found, success will be reported, including
the content of the first ``<required-var>``.  On repeated CMake runs,
the same message will not be printed again.

.. note::

  If ``<PackageName>`` does not match ``CMAKE_FIND_PACKAGE_NAME`` for the
  calling module, a warning that there is a mismatch is given. The
  ``FPHSA_NAME_MISMATCHED`` variable may be set to bypass the warning if using
  the old signature and the ``NAME_MISMATCHED`` argument using the new
  signature. To avoid forcing the caller to require newer versions of CMake for
  usage, the variable's value will be used if defined when the
  ``NAME_MISMATCHED`` argument is not passed for the new signature (but using
  both is an error)..

Example for the full signature:

.. code-block:: cmake

  find_package_handle_standard_args(LibArchive
    REQUIRED_VARS LibArchive_LIBRARY LibArchive_INCLUDE_DIR
    VERSION_VAR LibArchive_VERSION)

In this case, the ``LibArchive`` package is considered to be found if
both ``LibArchive_LIBRARY`` and ``LibArchive_INCLUDE_DIR`` are valid.
Also the version of ``LibArchive`` will be checked by using the version
contained in ``LibArchive_VERSION``.  Since no ``FAIL_MESSAGE`` is given,
the default messages will be printed.

Another example for the full signature:

.. code-block:: cmake

  find_package(Automoc4 QUIET NO_MODULE HINTS /opt/automoc4)
  find_package_handle_standard_args(Automoc4  CONFIG_MODE)

In this case, a ``FindAutmoc4.cmake`` module wraps a call to
``find_package(Automoc4 NO_MODULE)`` and adds an additional search
directory for ``automoc4``.  Then the call to
``find_package_handle_standard_args`` produces a proper success/failure
message.

.. command:: find_package_check_version

  .. versionadded:: 3.19

  Helper function which can be used to check if a ``<version>`` is valid
  against version-related arguments of :command:`find_package`.

  .. code-block:: cmake

    find_package_check_version(<version> <result-var>
      [HANDLE_VERSION_RANGE]
      [RESULT_MESSAGE_VARIABLE <message-var>]
      )

  The ``<result-var>`` will hold a boolean value giving the result of the check.

  The options are:

  ``HANDLE_VERSION_RANGE``
    Enable handling of a version range, if one is specified. Without this
    option, a developer warning will be displayed if a version range is
    specified.

  ``RESULT_MESSAGE_VARIABLE <message-var>``
    Specify a variable to get back a message describing the result of the check.

Example for the usage:

.. code-block:: cmake

  find_package_check_version(1.2.3 result HANDLE_VERSION_RANGE
    RESULT_MESSAGE_VARIABLE reason)
  if (result)
    message (STATUS "${reason}")
  else()
    message (FATAL_ERROR "${reason}")
  endif()
#]=======================================================================]

include(${CMAKE_CURRENT_LIST_DIR}/FindPackageMessage.cmake)


cmake_policy(PUSH)
# numbers and boolean constants
cmake_policy (SET CMP0012 NEW)
# IN_LIST operator
cmake_policy (SET CMP0057 NEW)


# internal helper macro
macro(_FPHSA_FAILURE_MESSAGE _msg)
  set (__msg "${_msg}")
  if (FPHSA_REASON_FAILURE_MESSAGE)
    string(APPEND __msg "\n    Reason given by package: ${FPHSA_REASON_FAILURE_MESSAGE}\n")
  endif()
  if (${_NAME}_FIND_REQUIRED)
    message(FATAL_ERROR "${__msg}")
  else ()
    if (NOT ${_NAME}_FIND_QUIETLY)
      message(STATUS "${__msg}")
    endif ()
  endif ()
endmacro()


# internal helper macro to generate the failure message when used in CONFIG_MODE:
macro(_FPHSA_HANDLE_FAILURE_CONFIG_MODE)
  # <PackageName>_CONFIG is set, but FOUND is false, this means that some other of the REQUIRED_VARS was not found:
  if(${_NAME}_CONFIG)
    _FPHSA_FAILURE_MESSAGE("${FPHSA_FAIL_MESSAGE}: missing:${MISSING_VARS} (found ${${_NAME}_CONFIG} ${VERSION_MSG})")
  else()
    # If _CONSIDERED_CONFIGS is set, the config-file has been found, but no suitable version.
    # List them all in the error message:
    if(${_NAME}_CONSIDERED_CONFIGS)
      set(configsText "")
      list(LENGTH ${_NAME}_CONSIDERED_CONFIGS configsCount)
      math(EXPR configsCount "${configsCount} - 1")
      foreach(currentConfigIndex RANGE ${configsCount})
        list(GET ${_NAME}_CONSIDERED_CONFIGS ${currentConfigIndex} filename)
        list(GET ${_NAME}_CONSIDERED_VERSIONS ${currentConfigIndex} version)
        string(APPEND configsText "\n    ${filename} (version ${version})")
      endforeach()
      if (${_NAME}_NOT_FOUND_MESSAGE)
        if (FPHSA_REASON_FAILURE_MESSAGE)
          string(PREPEND FPHSA_REASON_FAILURE_MESSAGE "${${_NAME}_NOT_FOUND_MESSAGE}\n    ")
        else()
          set(FPHSA_REASON_FAILURE_MESSAGE "${${_NAME}_NOT_FOUND_MESSAGE}")
        endif()
      else()
        string(APPEND configsText "\n")
      endif()
      _FPHSA_FAILURE_MESSAGE("${FPHSA_FAIL_MESSAGE} ${VERSION_MSG}, checked the following files:${configsText}")

    else()
      # Simple case: No Config-file was found at all:
      _FPHSA_FAILURE_MESSAGE("${FPHSA_FAIL_MESSAGE}: found neither ${_NAME}Config.cmake nor ${_NAME_LOWER}-config.cmake ${VERSION_MSG}")
    endif()
  endif()
endmacro()


function(FIND_PACKAGE_CHECK_VERSION version result)
  cmake_parse_arguments (PARSE_ARGV 2 FPCV "HANDLE_VERSION_RANGE;NO_AUTHOR_WARNING_VERSION_RANGE" "RESULT_MESSAGE_VARIABLE" "")

  if (FPCV_UNPARSED_ARGUMENTS)
    message (FATAL_ERROR "find_package_check_version(): ${FPCV_UNPARSED_ARGUMENTS}: unexpected arguments")
  endif()
  if ("RESULT_MESSAGE_VARIABLE" IN_LIST FPCV_KEYWORDS_MISSING_VALUES)
    message (FATAL_ERROR "find_package_check_version(): RESULT_MESSAGE_VARIABLE expects an argument")
  endif()

  set (${result} FALSE PARENT_SCOPE)
  if (FPCV_RESULT_MESSAGE_VARIABLE)
    unset (${FPCV_RESULT_MESSAGE_VARIABLE} PARENT_SCOPE)
  endif()

  if (_CMAKE_FPHSA_PACKAGE_NAME)
    set (package "${_CMAKE_FPHSA_PACKAGE_NAME}")
  elseif (CMAKE_FIND_PACKAGE_NAME)
    set (package "${CMAKE_FIND_PACKAGE_NAME}")
  else()
    message (FATAL_ERROR "find_package_check_version(): Cannot be used outside a 'Find Module'")
  endif()

  if (NOT FPCV_NO_AUTHOR_WARNING_VERSION_RANGE
      AND ${package}_FIND_VERSION_RANGE AND NOT FPCV_HANDLE_VERSION_RANGE)
    message(AUTHOR_WARNING
      "`find_package()` specify a version range but the option "
      "HANDLE_VERSION_RANGE` is not passed to `find_package_check_version()`. "
      "Only the lower endpoint of the range will be used.")
  endif()


  set (version_ok FALSE)
  unset (version_msg)

  if (FPCV_HANDLE_VERSION_RANGE AND ${package}_FIND_VERSION_RANGE)
    if ((${package}_FIND_VERSION_RANGE_MIN STREQUAL "INCLUDE"
          AND version VERSION_GREATER_EQUAL ${package}_FIND_VERSION_MIN)
        AND ((${package}_FIND_VERSION_RANGE_MAX STREQUAL "INCLUDE"
            AND version VERSION_LESS_EQUAL ${package}_FIND_VERSION_MAX)
          OR (${package}_FIND_VERSION_RANGE_MAX STREQUAL "EXCLUDE"
            AND version VERSION_LESS ${package}_FIND_VERSION_MAX)))
      set (version_ok TRUE)
      set(version_msg "(found suitable version \"${version}\", required range is \"${${package}_FIND_VERSION_RANGE}\")")
    else()
      set(version_msg "Found unsuitable version \"${version}\", required range is \"${${package}_FIND_VERSION_RANGE}\"")
    endif()
  elseif (DEFINED ${package}_FIND_VERSION)
    if(${package}_FIND_VERSION_EXACT)       # exact version required
      # count the dots in the version string
      string(REGEX REPLACE "[^.]" "" version_dots "${version}")
      # add one dot because there is one dot more than there are components
      string(LENGTH "${version_dots}." version_dots)
      if (version_dots GREATER ${package}_FIND_VERSION_COUNT)
        # Because of the C++ implementation of find_package() ${package}_FIND_VERSION_COUNT
        # is at most 4 here. Therefore a simple lookup table is used.
        if (${package}_FIND_VERSION_COUNT EQUAL 1)
          set(version_regex "[^.]*")
        elseif (${package}_FIND_VERSION_COUNT EQUAL 2)
          set(version_regex "[^.]*\\.[^.]*")
        elseif (${package}_FIND_VERSION_COUNT EQUAL 3)
          set(version_regex "[^.]*\\.[^.]*\\.[^.]*")
        else()
          set(version_regex "[^.]*\\.[^.]*\\.[^.]*\\.[^.]*")
        endif()
        string(REGEX REPLACE "^(${version_regex})\\..*" "\\1" version_head "${version}")
        if (NOT ${package}_FIND_VERSION VERSION_EQUAL version_head)
          set(version_msg "Found unsuitable version \"${version}\", but required is exact version \"${${package}_FIND_VERSION}\"")
        else ()
          set(version_ok TRUE)
          set(version_msg "(found suitable exact version \"${_FOUND_VERSION}\")")
        endif ()
      else ()
        if (NOT ${package}_FIND_VERSION VERSION_EQUAL version)
          set(version_msg "Found unsuitable version \"${version}\", but required is exact version \"${${package}_FIND_VERSION}\"")
        else ()
          set(version_ok TRUE)
          set(version_msg "(found suitable exact version \"${version}\")")
        endif ()
      endif ()
    else()     # minimum version
      if (${package}_FIND_VERSION VERSION_GREATER version)
        set(version_msg "Found unsuitable version \"${version}\", but required is at least \"${${package}_FIND_VERSION}\"")
      else()
        set(version_ok TRUE)
        set(version_msg "(found suitable version \"${version}\", minimum required is \"${${package}_FIND_VERSION}\")")
      endif()
    endif()
  else ()
    set(version_ok TRUE)
    set(version_msg "(found version \"${version}\")")
  endif()

  set (${result} ${version_ok} PARENT_SCOPE)
  if (FPCV_RESULT_MESSAGE_VARIABLE)
    set (${FPCV_RESULT_MESSAGE_VARIABLE} "${version_msg}" PARENT_SCOPE)
  endif()
endfunction()


function(FIND_PACKAGE_HANDLE_STANDARD_ARGS _NAME _FIRST_ARG)

  # Set up the arguments for `cmake_parse_arguments`.
  set(options  CONFIG_MODE  HANDLE_COMPONENTS NAME_MISMATCHED HANDLE_VERSION_RANGE)
  set(oneValueArgs  FAIL_MESSAGE  REASON_FAILURE_MESSAGE VERSION_VAR  FOUND_VAR)
  set(multiValueArgs REQUIRED_VARS)

  # Check whether we are in 'simple' or 'extended' mode:
  set(_KEYWORDS_FOR_EXTENDED_MODE  ${options} ${oneValueArgs} ${multiValueArgs} )
  list(FIND _KEYWORDS_FOR_EXTENDED_MODE "${_FIRST_ARG}" INDEX)

  unset(FPHSA_NAME_MISMATCHED_override)
  if (DEFINED FPHSA_NAME_MISMATCHED)
    # If the variable NAME_MISMATCHED variable is set, error if it is passed as
    # an argument. The former is for old signatures, the latter is for new
    # signatures.
    list(FIND ARGN "NAME_MISMATCHED" name_mismatched_idx)
    if (NOT name_mismatched_idx EQUAL "-1")
      message(FATAL_ERROR
        "The `NAME_MISMATCHED` argument may only be specified by the argument or "
        "the variable, not both.")
    endif ()

    # But use the variable if it is not an argument to avoid forcing minimum
    # CMake version bumps for calling modules.
    set(FPHSA_NAME_MISMATCHED_override "${FPHSA_NAME_MISMATCHED}")
  endif ()

  if(${INDEX} EQUAL -1)
    set(FPHSA_FAIL_MESSAGE ${_FIRST_ARG})
    set(FPHSA_REQUIRED_VARS ${ARGN})
    set(FPHSA_VERSION_VAR)
  else()
    cmake_parse_arguments(FPHSA "${options}" "${oneValueArgs}" "${multiValueArgs}"  ${_FIRST_ARG} ${ARGN})

    if(FPHSA_UNPARSED_ARGUMENTS)
      message(FATAL_ERROR "Unknown keywords given to FIND_PACKAGE_HANDLE_STANDARD_ARGS(): \"${FPHSA_UNPARSED_ARGUMENTS}\"")
    endif()

    if(NOT FPHSA_FAIL_MESSAGE)
      set(FPHSA_FAIL_MESSAGE  "DEFAULT_MSG")
    endif()

    # In config-mode, we rely on the variable <PackageName>_CONFIG, which is set by find_package()
    # when it successfully found the config-file, including version checking:
    if(FPHSA_CONFIG_MODE)
      list(INSERT FPHSA_REQUIRED_VARS 0 ${_NAME}_CONFIG)
      list(REMOVE_DUPLICATES FPHSA_REQUIRED_VARS)
      set(FPHSA_VERSION_VAR ${_NAME}_VERSION)
    endif()

    if(NOT FPHSA_REQUIRED_VARS AND NOT FPHSA_HANDLE_COMPONENTS)
      message(FATAL_ERROR "No REQUIRED_VARS specified for FIND_PACKAGE_HANDLE_STANDARD_ARGS()")
    endif()
  endif()

  if (DEFINED FPHSA_NAME_MISMATCHED_override)
    set(FPHSA_NAME_MISMATCHED "${FPHSA_NAME_MISMATCHED_override}")
  endif ()

  if (DEFINED CMAKE_FIND_PACKAGE_NAME
      AND NOT FPHSA_NAME_MISMATCHED
      AND NOT _NAME STREQUAL CMAKE_FIND_PACKAGE_NAME)
    message(AUTHOR_WARNING
      "The package name passed to `find_package_handle_standard_args` "
      "(${_NAME}) does not match the name of the calling package "
      "(${CMAKE_FIND_PACKAGE_NAME}). This can lead to problems in calling "
      "code that expects `find_package` result variables (e.g., `_FOUND`) "
      "to follow a certain pattern.")
  endif ()

  if (${_NAME}_FIND_VERSION_RANGE AND NOT FPHSA_HANDLE_VERSION_RANGE)
    message(AUTHOR_WARNING
      "`find_package()` specify a version range but the module ${_NAME} does "
      "not support this capability. Only the lower endpoint of the range "
      "will be used.")
  endif()

  # to propagate package name to FIND_PACKAGE_CHECK_VERSION
  set(_CMAKE_FPHSA_PACKAGE_NAME "${_NAME}")

  # now that we collected all arguments, process them

  if("x${FPHSA_FAIL_MESSAGE}" STREQUAL "xDEFAULT_MSG")
    set(FPHSA_FAIL_MESSAGE "Could NOT find ${_NAME}")
  endif()

  if (FPHSA_REQUIRED_VARS)
    list(GET FPHSA_REQUIRED_VARS 0 _FIRST_REQUIRED_VAR)
  endif()

  string(TOUPPER ${_NAME} _NAME_UPPER)
  string(TOLOWER ${_NAME} _NAME_LOWER)

  if(FPHSA_FOUND_VAR)
    set(_FOUND_VAR_UPPER ${_NAME_UPPER}_FOUND)
    set(_FOUND_VAR_MIXED ${_NAME}_FOUND)
    if(FPHSA_FOUND_VAR STREQUAL _FOUND_VAR_MIXED  OR  FPHSA_FOUND_VAR STREQUAL _FOUND_VAR_UPPER)
      set(_FOUND_VAR ${FPHSA_FOUND_VAR})
    else()
      message(FATAL_ERROR "The argument for FOUND_VAR is \"${FPHSA_FOUND_VAR}\", but only \"${_FOUND_VAR_MIXED}\" and \"${_FOUND_VAR_UPPER}\" are valid names.")
    endif()
  else()
    set(_FOUND_VAR ${_NAME_UPPER}_FOUND)
  endif()

  # collect all variables which were not found, so they can be printed, so the
  # user knows better what went wrong (#6375)
  set(MISSING_VARS "")
  set(DETAILS "")
  # check if all passed variables are valid
  set(FPHSA_FOUND_${_NAME} TRUE)
  foreach(_CURRENT_VAR ${FPHSA_REQUIRED_VARS})
    if(NOT ${_CURRENT_VAR})
      set(FPHSA_FOUND_${_NAME} FALSE)
      string(APPEND MISSING_VARS " ${_CURRENT_VAR}")
    else()
      string(APPEND DETAILS "[${${_CURRENT_VAR}}]")
    endif()
  endforeach()
  if(FPHSA_FOUND_${_NAME})
    set(${_NAME}_FOUND TRUE)
    set(${_NAME_UPPER}_FOUND TRUE)
  else()
    set(${_NAME}_FOUND FALSE)
    set(${_NAME_UPPER}_FOUND FALSE)
  endif()

  # component handling
  unset(FOUND_COMPONENTS_MSG)
  unset(MISSING_COMPONENTS_MSG)

  if(FPHSA_HANDLE_COMPONENTS)
    foreach(comp ${${_NAME}_FIND_COMPONENTS})
      if(${_NAME}_${comp}_FOUND)

        if(NOT DEFINED FOUND_COMPONENTS_MSG)
          set(FOUND_COMPONENTS_MSG "found components:")
        endif()
        string(APPEND FOUND_COMPONENTS_MSG " ${comp}")

      else()

        if(NOT DEFINED MISSING_COMPONENTS_MSG)
          set(MISSING_COMPONENTS_MSG "missing components:")
        endif()
        string(APPEND MISSING_COMPONENTS_MSG " ${comp}")

        if(${_NAME}_FIND_REQUIRED_${comp})
          set(${_NAME}_FOUND FALSE)
          string(APPEND MISSING_VARS " ${comp}")
        endif()

      endif()
    endforeach()
    set(COMPONENT_MSG "${FOUND_COMPONENTS_MSG} ${MISSING_COMPONENTS_MSG}")
    string(APPEND DETAILS "[c${COMPONENT_MSG}]")
  endif()

  # version handling:
  set(VERSION_MSG "")
  set(VERSION_OK TRUE)

  # check with DEFINED here as the requested or found version may be "0"
  if (DEFINED ${_NAME}_FIND_VERSION)
    if(DEFINED ${FPHSA_VERSION_VAR})
      set(_FOUND_VERSION ${${FPHSA_VERSION_VAR}})
      if (FPHSA_HANDLE_VERSION_RANGE)
        set (FPCV_HANDLE_VERSION_RANGE HANDLE_VERSION_RANGE)
      else()
        set(FPCV_HANDLE_VERSION_RANGE NO_AUTHOR_WARNING_VERSION_RANGE)
      endif()
      find_package_check_version ("${_FOUND_VERSION}" VERSION_OK RESULT_MESSAGE_VARIABLE VERSION_MSG
        ${FPCV_HANDLE_VERSION_RANGE})
    else()
      # if the package was not found, but a version was given, add that to the output:
      if(${_NAME}_FIND_VERSION_EXACT)
        set(VERSION_MSG "(Required is exact version \"${${_NAME}_FIND_VERSION}\")")
      elseif (FPHSA_HANDLE_VERSION_RANGE AND ${_NAME}_FIND_VERSION_RANGE)
        set(VERSION_MSG "(Required is version range \"${${_NAME}_FIND_VERSION_RANGE}\")")
      else()
        set(VERSION_MSG "(Required is at least version \"${${_NAME}_FIND_VERSION}\")")
      endif()
    endif()
  else ()
    # Check with DEFINED as the found version may be 0.
    if(DEFINED ${FPHSA_VERSION_VAR})
      set(VERSION_MSG "(found version \"${${FPHSA_VERSION_VAR}}\")")
    endif()
  endif ()

  if(VERSION_OK)
    string(APPEND DETAILS "[v${${FPHSA_VERSION_VAR}}(${${_NAME}_FIND_VERSION})]")
  else()
    set(${_NAME}_FOUND FALSE)
  endif()


  # print the result:
  if (${_NAME}_FOUND)
    FIND_PACKAGE_MESSAGE(${_NAME} "Found ${_NAME}: ${${_FIRST_REQUIRED_VAR}} ${VERSION_MSG} ${COMPONENT_MSG}" "${DETAILS}")
  else ()

    if(FPHSA_CONFIG_MODE)
      _FPHSA_HANDLE_FAILURE_CONFIG_MODE()
    else()
      if(NOT VERSION_OK)
        set(RESULT_MSG)
        if (_FIRST_REQUIRED_VAR)
          string (APPEND RESULT_MSG "found ${${_FIRST_REQUIRED_VAR}}")
        endif()
        if (COMPONENT_MSG)
          if (RESULT_MSG)
            string (APPEND RESULT_MSG ", ")
          endif()
          string (APPEND RESULT_MSG "${FOUND_COMPONENTS_MSG}")
        endif()
        _FPHSA_FAILURE_MESSAGE("${FPHSA_FAIL_MESSAGE}: ${VERSION_MSG} (${RESULT_MSG})")
      else()
        _FPHSA_FAILURE_MESSAGE("${FPHSA_FAIL_MESSAGE} (missing:${MISSING_VARS}) ${VERSION_MSG}")
      endif()
    endif()

  endif ()

  set(${_NAME}_FOUND ${${_NAME}_FOUND} PARENT_SCOPE)
  set(${_NAME_UPPER}_FOUND ${${_NAME}_FOUND} PARENT_SCOPE)
endfunction()


cmake_policy(POP)

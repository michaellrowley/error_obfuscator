# error_obfuscator
*A cross-platform enum obfuscation tool*

</br>

This project is being developed as an error-code obfuscation tool suitable for C, C++, and C# source code files alike (due to their similar approaches when it comes to enums) - there are a few things that users should be aware of before using the program to avoid mistakes and to make sure that they have a good experience using it, which is why this readme exists!

---
</br>

## Examples:


Before:
```C
enum age { old, new };
```

After:
```C
enum age {
	old = -5474,
	new = 968
};
```

---
</br>

## Usage:

### Command-line:
``[ [error_obfuscator] -f [path] [optional] ]``

(For example, to obfuscate all enums in the directory "``src/``" a user could run ``error_obfuscator.exe -f src/`` and to restore the original source-code files, one could run ``error_obfuscator.exe -f src/ -r``.)

Where **``error_obfuscator``** is your platform-specific execution line for your error_obfuscator instance (e.g ``error_obfuscator.exe`` or ``./error_obfuscator``) and ``optional`` refers to any optional parameters that the tool might accept, all of which are listed below:

| Name | Description | Flag |
|-|-|-|
| ``-r``/``--restore`` | Restores all ``.bkup`` files from the provided directory instead of obfuscating the applicable ones | Yes |

*(Flags do not require additional data whereas non-flags require a value to be provided after them to be used during execution.)*

If no optional parameters are provided that adjust the program's default role, it will obfuscate all enums under the applicable target path.
</br>

### As a Visual Studio build event:
Simply follow [Microsoft's guide to using build events](https://docs.microsoft.com/en-us/visualstudio/ide/how-to-specify-build-events-csharp) and, in place of step four which says:

> In the **Pre-build event command line** box, specify the syntax of the build event.

Use the syntax as explained in the above 'Command-Line' section in addition to using [build event macros](https://www.dotnetperls.com/post-pre-build-macros).

---

## Compilation:

This is a cross-platform tool, meaning that it supports both Windows and Linux native compilers (``G++`` & ``CL``) - here are the compilation instructions:

### Windows (CL):
- Open the [Visual Studio Developer Command Prompt]](https://docs.microsoft.com/en-us/cpp/build/building-on-the-command-line?view=msvc-160) as an Administrator.

- In the developer command prompt, execute the following command to download the project's source code:
```CMD
curl --output error_obfuscator.cpp --url https://raw.githubusercontent.com/michaellrowley/error_obfuscator/main/src/main.cpp
```

- Use the ``dir`` command to ensure that there is now a file under the title of ``error_obfuscator.cpp`` in your console's directory.

- Execute the following command in your console:
```CMD
cl error_obfuscator.cpp
```

---
</br>

### Linux/Windows (G++):

- In a linux terminal, execute the below commands (``git-clone`` & ``wget`` would work too):
```Terminal
curl --output error_obfuscator.cpp --url https://raw.githubusercontent.com/michaellrowley/error_obfuscator/main/src/main.cpp
g++ error_obfuscator.cpp -o error_obfuscator
```

---
</br>

## Edge cases:
I've tried to throw edge cases at this tool and some have been successful and some have caused errors, when errors occur I try to patch them or at a bare minimum figure out why they exist (I used ``gdb`` for debugging so if you are able to recreate an issue that isn't mentioned here in any other debugging environment please let me know as I might need to download additional tools to patch it).

Here are the known edge cases that need patching in the future:

| Edge case description | Fixed | Priority |
|----|----|----|
| Line-wide comments (``//``) in enums aren't handled properly when recreating ENUMSTRUCTs | No | Low |

In addition to the above edge cases, this program should not bed used on more than ``INT_MAX`` files or on files with more than ``INT_MAX`` enums.

---
</br>

## How it works:

The obfuscation code takes into account an enum-value's original value when giving it a new one so that positive enum values get positive random ones and negative ones get negative random values, this ensures that enum values remain conformant with generic success/failure macros that 
the user might have implemented.
```C++
enum EState {			 ->	enum EState {
	ERROR_FAILED = -1,	 ->		ERROR_FAILED = -93568,
	ERROR_EXCEPTION = -2,	 ->		ERROR_EXCEPTION = -425,
	ERROR_NORAM = -3,	 ->		ERROR_NORAM = -383697,
	ERROR_CRASH = -4,	 ->		ERROR_CRASH = -6863,
	SUCCESS = 1		 ->		SUCCESS = 4956
}				 ->	}
```

In addition to this, the obfuscation code also identifies multiple enums with the same value and resolves their new 'random' value to the same one throughout all identical values:
```C++

enum EState {		->	enum EState {
	RUNNING = 1,	->		RUNNING = 108525,
	WALKING = 2,	->		WALKING = 28367,
	SPRINTING = 1,	->		SPRINTING = 108525,
	CLIMBING = 3	->		CLIMBING = 58396868
}			->	}

```

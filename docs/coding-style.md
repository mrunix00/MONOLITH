## Coding style (Draft)

For all code formatting, all code should follow the format specified in `.clang-format` file in the project root.

### Naming conventions

- Use `snake_case` for variable and function names.
- Use `SCREAMING_SNAKE_CASE` for constants.
- Append `_t` to the end of type names.
- Prepend `_` to the beginning of private functions and static global variables.

###### Right

```c
int my_variable;
void my_function() {}
typedef struct
{
    int my_field;
} my_struct_t;
typedef enum
{
    MY_ENUM_1,
    MY_ENUM_2,
} my_enum_t;
#define MY_CONSTANT 42
static int _my_static_variable;
static void _helper_function() {}
```

###### Wrong

```c
int MyVariable;
void MyFunction() {}
typedef struct {
    int MyField;
} my_struct;
typedef enum {
    MyEnum1,
    MyEnum2,
} MyEnum;
#define MyConstant 42
```

- Use descriptive names for variables, functions, types, etc.
- Avoid using single-letter names for variables, functions, types, etc.

###### Right

```c
int count;
void print_error_message() {}
typedef struct
{
    int value;
} device_descriptor_t;
```

###### Wrong

```c
int n;
void perr() {}
typedef struct {
    int v;
} dd_t;
```

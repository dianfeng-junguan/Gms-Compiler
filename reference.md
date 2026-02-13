# 规范
## ast node的构成

### NODE_DEFINITION

- left: 
  - left:NODE_ID
  - right:NODE_TYPEKW|NULL
- right: expr

### NODE_FUNCTION

- left: 
  - left: NODE_ID
  - right: NODE_TYPEKW
- right:
  - left: NODE_ARGLIST
  - right: statements

### NODE\_VAR\_DECL

- left: NODE_ID
- right: NODE_TYPEKW

### NODE\_FUNC\_DECL

- left: 
  - left: NODE_ID
  - right: NODE_TYPEKW return type
- right: NODE_ARGLIST

### NODE_CLASS

- left: NODE_ID
- right: NODE\_LEAFHOLDER NODE\_CLASSMEMBER members

### NODE_ARGPAIR

- left: NODE_ID
- right: NODE_TYPEKW

## class的定义
```
let a:classname=classname{name:value,...};
```

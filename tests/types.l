(include "core/assert.l")

;; nil-t
(assert-type nil nil-t)

;; type-t
(assert-type nil-t type-t)
(assert-type type-t type-t)
(assert-type error-t type-t)
(assert-type str-t type-t)
(assert-type int64-t type-t)
(assert-type bool-t type-t)
(assert-type sym-t type-t)
(assert-type builtin-t type-t)
(assert-type fn-t type-t)
(assert-type list-t type-t)

;; error-t
(assert-type undefinedsymbol error-t)

;; str-t
(assert-type "hello" str-t)

;; int64-t
(assert-type 0 int64-t)
(assert-type 1 int64-t)
(assert-type 1234 int64-t)
(assert-type -1 int64-t)
(assert-type -1234 int64-t)
(assert-type 9223372036854775807 int64-t)
(assert-type -9223372036854775807 int64-t)
(assert-type -9223372036854775808 int64-t)

;; bool-t
(assert-type false bool-t)
(assert-type true bool-t)

;; sym-t
(assert-type 'a sym-t)
(assert-type 'a12 sym-t)
(assert-type 'a12b sym-t)
(assert-type 'nil sym-t)
(assert-type 'fn-t sym-t)
(assert-type 'str? sym-t)

;; builtin-t
(assert-type print builtin-t)
(assert-type if builtin-t)
(assert-type fn builtin-t)

;; fn-t
(assert-type assert-type fn-t)
(assert-type include fn-t)
(assert-type nil? fn-t)

;; list-t
(assert-type '(1 2 3) list-t)
(assert-type (tail '(1 2 3)) list-t)

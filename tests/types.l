(setg assert-types
      (fn (x t (rest xs))
          (if (= (typeof x) t)
              (if (nil? xs) nil (assert-types (unpack xs)))
              (list "expected type" t x))))

(assert-types

 nil nil-t

 nil-t type-t
 type-t type-t
 error-t type-t
 str-t type-t
 int64-t type-t
 bool-t type-t
 sym-t type-t
 builtin-t type-t
 fn-t type-t
 list-t type-t

 undefinedsymbol error-t

 "hello" str-t

 0 int64-t
 1 int64-t
 1234 int64-t
 18446744073709551615 int64-t

 false bool-t
 true bool-t

 (quote a) sym-t
 (quote a12) sym-t
 (quote a12b) sym-t
 (quote nil) sym-t
 (quote fn-t) sym-t
 (quote str?) sym-t

 print builtin-t
 if builtin-t
 fn builtin-t

 assert-types fn-t
 include fn-t
 nil? fn-t

 (list 1 2 3) list-t
 (tail (list 1 2 3)) list-t

 )

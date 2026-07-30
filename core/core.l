(setg include (fn (file-name) (eval (unpack (parse-lisp (read-file file-name))))))

(setg not (fn (a) (if a false true)))
(setg and (fn (a b) (if a b false)))
(setg or (fn (a b) (if a true b)))

(setg <= (fn (a b) (not (< b a))))
(setg > (fn (a b) (< b a)))
(setg >= (fn (a b) (not (< a b))))

(setg != (fn (a b) (not (= a b))))

(setg nil? (fn (x) (= (typeof x) nil-t)))
(setg type? (fn (x) (= (typeof x) type-t)))
(setg error? (fn (x) (= (typeof x) error-t)))
(setg str? (fn (x) (= (typeof x) str-t)))
(setg int64? (fn (x) (= (typeof x) int64-t)))
(setg bool? (fn (x) (= (typeof x) bool-t)))
(setg sym? (fn (x) (= (typeof x) sym-t)))
(setg builtin? (fn (x) (= (typeof x) builtin-t)))
(setg fn? (fn (x) (= (typeof x) fn-t)))
(setg list? (fn (x) (= (typeof x) list-t)))

(setg concat (fn (l1 l2) (if (nil? l1) l2
                             (if (list? l1) (cons (head l1) (concat (tail l1) l2))
                                 (error 'l1-not-list)))))


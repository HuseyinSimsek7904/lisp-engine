(setg include (macro (file-name)
                     `(do ~@(parse-lisp (read-file file-name)))))

(setg >  (fn (a b) (< b a)))
(setg <= (fn (a b) (not (< b a))))
(setg >= (fn (a b) (not (< a b))))

(setg != (fn (a b) (not (= a b))))

(setg nil?     (fn (x) (= x nil)))
(setg true?    (fn (x) (= x true)))
(setg false?   (fn (x) (= x false)))

(setg type?    (fn (x) (= (typeof x) type-t)))
(setg error?   (fn (x) (= (typeof x) error-t)))
(setg str?     (fn (x) (= (typeof x) str-t)))
(setg int64?   (fn (x) (= (typeof x) int64-t)))
(setg bool?    (fn (x) (= (typeof x) bool-t)))
(setg sym?     (fn (x) (= (typeof x) sym-t)))
(setg builtin? (fn (x) (= (typeof x) builtin-t)))
(setg fn?      (fn (x) (= (typeof x) fn-t)))
(setg list?    (fn (x) (= (typeof x) list-t)))

(setg concat (fn (l1 l2) (if (nil? l1) l2
                             (if (list? l1) (cons (head l1) (concat (tail l1) l2))
                                 (error 'l1-not-list)))))

(setg not (fn (c) (if c false true)))
(setg and (macro ((rest cs)) (if (nil? cs) true
                                 `(if ~(head cs)
                                      (and ~@(tail cs))
                                      false))))
(setg or (macro ((rest cs)) (if (nil? cs) false
                                `(if ~(head cs)
                                     true
                                     (or ~@(tail cs))))))

(setg cond (macro ((rest ps)) (if (nil? ps) nil
                                  `(if ~(head (head ps))
                                       ~(head (head ps) 1)
                                       (cond ~@(tail ps))))))

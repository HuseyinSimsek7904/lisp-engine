(set include (macro (file-name)
                    `(do ~@(parse-lisp (read-file file-name)))))

(set >  (fn (a b) (< b a)))
(set <= (fn (a b) (not (< b a))))
(set >= (fn (a b) (not (< a b))))

(set != (fn (a b) (not (= a b))))

(set nil?     (fn (x) (= x nil)))
(set true?    (fn (x) (= x true)))
(set false?   (fn (x) (= x false)))

(set nil?     (fn (x) (= x nil)))
(set true?    (fn (x) (= x true)))
(set false?   (fn (x) (= x false)))

(set type?    (fn (x) (= (typeof x) type-t)))
(set error?   (fn (x) (= (typeof x) error-t)))
(set str?     (fn (x) (= (typeof x) str-t)))
(set int64?   (fn (x) (= (typeof x) int64-t)))
(set bool?    (fn (x) (= (typeof x) bool-t)))
(set sym?     (fn (x) (= (typeof x) sym-t)))
(set builtin? (fn (x) (= (typeof x) builtin-t)))
(set fn?      (fn (x) (= (typeof x) fn-t)))
(set list?    (fn (x) (= (typeof x) list-t)))

(set concat (fn (l1 l2) (if (nil? l1) l2
                            (if (list? l1) (cons (head l1) (concat (tail l1) l2))
                                (error 'l1-not-list)))))

(set not (fn (c) (if c false true)))
(set and (macro ((rest cs)) (if (nil? cs) true
                                `(if ~(head cs)
                                     (and ~@(tail cs))
                                     false))))
(set or (macro ((rest cs)) (if (nil? cs) false
                               `(if ~(head cs)
                                    true
                                    (or ~@(tail cs))))))

(set cond (macro ((rest ps)) (if (nil? ps) nil
                                 `(if ~(head (head ps))
                                      ~(head (head ps) 1)
                                      (cond ~@(tail ps))))))

;; List utilities
(set last (fn (l) (cond ((nil? l) (error 'list-empty))
                        ((not (list? l)) (error 'not-a-list))
                        ((nil? (tail l)) (head l))
                        (true (last (tail l))))))

(set init (fn (l) (cond ((nil? l) (error 'list-empty))
                        ((not (list? l)) (error 'not-a-list))
                        ((nil? (tail l)) nil)
                        (true (cons (head l) (init (tail l)))))))

(set reverse (fn (l) (cond ((nil? l) nil)
                           ((not (list? l)) (error 'not-a-list))
                           (true (cons (last l) (reverse (init l)))))))

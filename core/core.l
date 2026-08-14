;; TODO: This must use absolute paths.
(set included (atom '("core/core.l")))
(set include (macro (file-name)
                    (if (contains? file-name (deref included)) nil
                        (reset included (cons file-name (deref included)))
                        `(do ~@(parse-lisp (read-file file-name))))))

(set eval-file (macro (file-name)
                      `(do ~@(parse-lisp (read-file file-name)))))

(set >  (fn (a b) (< b a)))
(set <= (fn (a b) (not (< b a))))
(set >= (fn (a b) (not (< a b))))

(set range? (fn (l h x) (and (>= x l) (<= x h))))

(set != (fn (a b) (not (= a b))))

(set nil?     (fn (x) (= x nil)))
(set true?    (fn (x) (= x true)))
(set false?   (fn (x) (= x false)))
(set empty?   (fn (x) (= x empty)))

(set type?    (fn (x) (= (typeof x) type-t)))
(set error?   (fn (x) (= (typeof x) error-t)))
(set str?     (fn (x) (= (typeof x) str-t)))
(set int64?   (fn (x) (= (typeof x) int64-t)))
(set bool?    (fn (x) (= (typeof x) bool-t)))
(set sym?     (fn (x) (= (typeof x) sym-t)))
(set builtin? (fn (x) (= (typeof x) builtin-t)))
(set fn?      (fn (x) (= (typeof x) fn-t)))
(set list?    (fn (x) (= (typeof x) list-t)))

(set concat (fn (l1 l2)
                (if (not (list? l1)) (error 'concat-l1-not-list))
                (if (empty? l1) l2
                    (cons (head l1) (concat (tail l1) l2)))))

(set not (fn (c) (if c false true)))
(set and (macro ((rest cs)) (if (empty? cs) true
                                `(if ~(head cs)
                                     (and ~@(tail cs))
                                     false))))
(set or (macro ((rest cs)) (if (empty? cs) false
                               `(if ~(head cs)
                                    true
                                    (or ~@(tail cs))))))

(set cond (macro ((rest ps)) (if (empty? ps) nil
                                 `(if (not ~(head (head ps)))
                                      (cond ~@(tail ps))
                                      ~(head (head ps) 1)))))

(set case (macro (expr (rest cs))
                 ;; TODO: This can be implemented using a labelled `let` binding.
                 ;; The language currently does not implement labelled bindings.
                 (set _case (fn (cs)
                                (if (empty? cs) nil
                                    `(if (!= x ~(head (head cs)))
                                         ~(_case (tail cs))
                                         ~@(tail (head cs))))))

                 `(let ((x ~expr))
                    ~(_case cs))))

(set switch (macro (x (rest cs)) (if (empty? cs) nil
                                     `(if (= ~x ~(head (head cs)))
                                          ~(head (head cs) 1)
                                          (switch ~x ~@(tail cs))))))

;; List utilities
(set contains? (fn (x l)
                   (if (not (list? l)) (error `(not-a-list contains? ~l)))
                   (and (not (empty? l))
                        (or (= x (head l))
                            (contains? x (tail l))))))

(set last (fn (l) (cond ((not (list? l)) (error `(not-a-list last ~l)))
                        ((empty? l) (error 'list-empty))
                        ((empty? (tail l)) (head l))
                        (true (last (tail l))))))

(set init (fn (l) (cond ((not (list? l)) (error `(not-a-list init ~l)))
                        ((empty? l) (error 'list-empty))
                        ((empty? (tail l)) empty)
                        (true (cons (head l) (init (tail l)))))))

(set reverse (fn (l) (cond ((not (list? l)) (error `(not-a-list reverse ~l)))
                           ((empty? l) empty)
                           (true (cons (last l) (reverse (init l)))))))

(set map (fn (f l) (cond ((not (list? l)) (error `(not-a-list map ~l)))
                         ((empty? l) empty)
                         (true (cons (f (head l)) (map f (tail l)))))))

(set len (fn (l) (cond ((not (list? l)) (error `(not-a-list len ~l)))
                       ((empty? l) 0)
                       (true (+ 1 (len (tail l)))))))

(set any (fn (l) (cond ((not (list? l)) (error `(not-a-list any ~l)))
                       ((empty? l) false)
                       (true (if (head l) true (any (tail l)))))))

(set all (fn (l) (cond ((not (list? l)) (error `(not-a-list all ~l)))
                       ((empty? l) true)
                       (true (if (head l) (all (tail l)) false)))))

(set printf (macro (f (rest r))
                   `(print (format ~f ~@r))))

(set repeat (fn (x n) (cond ((<= n 0) '())
                            (true (cons x (repeat x (- n 1)))))))

(set min2 (fn (a b) (if (< a b) a b)))
(set max2 (fn (a b) (if (< a b) b a)))

(set min:list (fn (l) (cond ((not (list? l)) (error 'not-a-list))
                            ((empty? l) (error 'list-empty))
                            ((= 1 (len l)) (head l))
                            (true (min2 (head l) (max:list (tail l)))))))

(set max:list (fn (l) (cond ((not (list? l)) (error 'not-a-list))
                            ((empty? l) (error 'list-empty))
                            ((= 1 (len l)) (head l))
                            (true (max2 (head l) (max:list (tail l)))))))

(set min (fn ((rest xs)) (min:list xs)))
(set max (fn ((rest xs)) (max:list xs)))

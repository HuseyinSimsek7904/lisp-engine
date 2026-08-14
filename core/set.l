(set set:from-list (fn (l)
                       (cond ((not (list? l)) (error `(not-a-list set:from-list ~l)))
                             ((empty? l) empty)
                             ((set:contains? (head l) (tail l)) (set:from-list (tail l)))
                             (true (cons (head l) (set:from-list (tail l)))))))

(set set:make (fn ((rest r)) (set:from-list r)))

(set set:add (fn (x set) (if (set:contains? x set) set (cons x set))))

(set set:delete (fn (x set) (if (not (set:contains? x set)) set
                                (if (= x (head set))
                                    (set:delete x (tail set))
                                    (cons (head set) (set:delete x (tail set)))))))

(set set:union (fn (s1 s2) (if (empty? s1) s2
                               (set:add (head s1) (set:union (tail s1) s2)))))

(set set:inter (fn (s1 s2) (if (empty? s1) empty
                               (if (set:contains? (head s1) s2)
                                   (set:add (head s1) (set:inter (tail s1) s2))
                                   (set:inter (tail s1) s2)))))

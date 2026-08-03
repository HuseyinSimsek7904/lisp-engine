(set quasi-quote-items-fn (fn (exprs)
                              (if (empty? exprs) 'empty
                                  (if (= 'unquote-expand (head (head exprs)))
                                      (list 'concat
                                            (head (head exprs) 1)
                                            (quasi-quote-items-fn (tail exprs)))
                                      (list 'cons
                                            (quasi-quote-fn (head exprs))
                                            (quasi-quote-items-fn (tail exprs)))))))

(set quasi-quote-fn (fn (expr)
                        (if (list? expr)
                            (if (empty? expr) 'empty
                                (if (= 'unquote (head expr))
                                    (head expr 1)
                                    (quasi-quote-items-fn expr)))
                            (list 'quote expr))))

(set quasi-quote (macro (expr) (quasi-quote-fn expr)))

(define double
	(lambda (x)
		(* 2 x)
	)
)
(define powof2
	(lambda (x)
		(if (= (logand x (- x 1)) 0)
			(if (not (= x 0))
				#t
				#f
			)
			#f
		)
	)
)
(define sum
	(lambda (li)
		(if (pair? li)
			(+ (car li) (sum (cdr li)))
			0
		)
	)
)
(define applyToList
	(lambda (f  l)
		(if (pair? l)
			(cons (f (car l)) (applyToList f (cdr l)))
			'()
		)
	)
)

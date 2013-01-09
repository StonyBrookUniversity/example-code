#use "scheme.ml"
#use "testUtils.ml"

let test_eval x = List.map (fun y -> 
        print_endline ("% " ^ (unparse y)) ;
	print_endline (string_of_value (eval y))) x
;;

let s_exprs = [
	(List [(Id "define");(Id "x");(Num 52)]) ;
	(List [(Id "define");(Id "foo");
		(List [(Id "lambda");
		(List [(Id "x")]);
		(List [(Id "lambda");
		(List [(Id "y")]);
		(List [(Id "+");(Id "x");(Id "y")])])])]) ;
	(List [(List [(Id "foo");(Num 3)]);(Num 4)]) ;
	(List [(Id "define");(Id "bar");
		(List [(Id "lambda");
		(List [(Id "y")]);
		(List [(Id "+");(Id "x");(Id "y")])])]) ;
	(List [(Id "bar");(Num 2)]) ;
	(List [(Id "define");(Id "x");(Num 7)]) ;
	(List [(Id "bar");(Num 2)]) ;
] ;; 
 
test_eval s_exprs ;;


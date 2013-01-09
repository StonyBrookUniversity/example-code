#use "scheme.ml"
#use "testUtils.ml"

let test_eval x = List.map (fun y -> 
	print_endline ("% " ^ (unparse y));
	print_endline (string_of_value (eval y))) x
;;

let s_exprs = [
	(List [(Id "boolean?");(Bool true)]) ;
	(List [(Id "boolean?");(Num 3)]) ;
	(List [(Id "define");(Id "three");(Num 3)]) ;
	(Id "three") ;
	(List [(Id "+");(Id "three");(Num 4)]) ;
	(List [(Id "define");(Id "add-two");
		(List [(Id "lambda");
		(List [(Id "n")]);
		(List [(Id "+");(Id "n");(Num 2)])])]) ;
	(List [(Id "add-two");(Num 5)]) ;
	(List [(Id "define");(Id "fact");
		(List [(Id "lambda");
		(List [(Id "n")]);
		(List [(Id "if");
		(List [(Id "=");(Id "n");(Num 0)]);(Num 1);
		(List [(Id "*");(Id "n");
		(List [(Id "fact");
		(List [(Id "-");(Id "n");(Num 1)])])])])])]) ;
	(List [(Id "fact");(Num 3)]) ;
	(List [(Id "fact");(Num 5)]) ;
] ;; 
 
test_eval s_exprs ;;

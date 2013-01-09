#use "scheme.ml"
#use "testUtils.ml"

let rec unparse_list2 = function
    [] -> ""
  | (x::[]) -> unparse2 x 
  | (x::xs) -> (unparse2 x) ^ ";" ^ (unparse_list2 xs) 
 
and unparse2 = function
  | Id id -> "(Id \"" ^ id ^ "\")"
  | Num n -> "(Num " ^ string_of_int n ^ ")"
  | Bool true -> "(Bool true)"
  | Bool false -> "(Bool false)"
  | String s -> "(String \"" ^ s ^ "\")"
  | List l -> "(List [" ^ unparse_list2 l ^ "])"
;;

let test_parse x = List.map (fun y -> 
	print_endline ("% " ^ y) ;
	print_endline (unparse2 (parse (tokenize y))))
	x
;;

let s_exprs = [
	"(boolean? #t)" ;
	"(boolean? 3)" ;
	"(define three 3)" ;
	"three" ;
	"(+ three 4)" ;
	"(define add-two (lambda (n) (+ n 2)))" ;
	"(add-two 5)" ;
	"(define fact (lambda (n) (if (= n 0) 1 (* n (fact (- n 1))))))" ;
	"(fact 3)" ;
	"(fact 5)" ;
] ;;

test_parse s_exprs ;;

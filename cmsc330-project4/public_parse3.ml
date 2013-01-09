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
	"(define x 52)" ;
	"(define foo (lambda (x) (lambda (y) (+ x y))))" ;
	"((foo 3) 4)" ;
	"(define bar (lambda (y) (+ x y)))" ;
	"(bar 2)" ;
	"(define x 7)" ;
	"(bar 2)" ;
	"(cons 1 nil)" ;
	"(cons 2 (cons 3 nil))" ;
	"(car (cons 4 (cons 5 nil)))" ;
	"(cdr (cons 6 (cons 7 nil)))" ;
	"(if #t 1 2)" ;
	"(if #f 1 2)" ;
] ;;

test_parse s_exprs ;;

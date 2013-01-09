(* Put your lexer, parser, and interpreter here *)

#load "str.cma"

(* Use this as your abstract syntax tree *)
type ast =
    Id of string
  | Num of int
  | Bool of bool
  | String of string
  | List of ast list

(* An unparser turns an AST back into a string.  You may find this
   unparser handy in writing this project *)
let rec unparse_list = function
    [] -> ""
  | (x::[]) -> unparse x
  | (x::xs) -> (unparse x) ^ " " ^ (unparse_list xs)

and unparse = function
  | Id id -> id
  | Num n -> string_of_int n
  | Bool true -> "#t"
  | Bool false -> "#f"
  | String s -> "\"" ^ s ^ "\""
  | List l -> "(" ^ unparse_list l ^ ")"

(************************************************************************)

(* Lexing *)

type token =
   TId of string
 | TNum of int
 | TString of string
 | TTrue
 | TFalse
 | TLParen
 | TRParen

let re_lparen = Str.regexp "("
let re_rparen = Str.regexp ")"
let re_id = Str.regexp "[a-zA-Z=*+/<>!?-][a-zA-Z0-9=*+/<>!?-]*"
let re_num = Str.regexp "[-]*[0-9]+"
let re_true = Str.regexp "#t"
let re_false = Str.regexp "#f"
let re_string = Str.regexp "\"[^\"]*\""
let re_whitespace = Str.regexp "[ \t\n]"

exception Lex_error of int

let tokenize s =
 let rec tokenize' pos s =
   if pos >= String.length s then
     []
   else begin
     if (Str.string_match re_lparen s pos) then
       TLParen::(tokenize' (pos+1) s)
     else if (Str.string_match re_rparen s pos) then
       TRParen::(tokenize' (pos+1) s)
     else if (Str.string_match re_true s pos) then
       TTrue::(tokenize' (pos+2) s)
     else if (Str.string_match re_false s pos) then
       TFalse::(tokenize' (pos+2) s)
     else if (Str.string_match re_id s pos) then
       let token = Str.matched_string s in
       let new_pos = Str.match_end () in
       (TId token)::(tokenize' new_pos s)
     else if (Str.string_match re_string s pos) then
       let token = Str.matched_string s in
       let new_pos = Str.match_end () in
       let tok = TString (String.sub token 1 ((String.length token)-2)) in
       tok::(tokenize' new_pos s)
     else if (Str.string_match re_num s pos) then
       let token = Str.matched_string s in
       let new_pos = Str.match_end () in
       (TNum (int_of_string token))::(tokenize' new_pos s)
     else if (Str.string_match re_whitespace s pos) then
       tokenize' (Str.match_end ()) s
     else
       raise (Lex_error pos)
   end
 in
 tokenize' 0 s

(************************************************************************)

(* Your parser goes here *)
exception Unknown_token;;
exception Invalid_expression of string;;
(* Your parser goes here *)
let rec parse_sexpr lookahead = 
	match !lookahead with
	| (TId(x)::t) 		-> lookahead := t; Id(x)
	| (TNum(x)::t) 		-> lookahead := t; Num(x)
	| (TString(x)::t) 	-> lookahead := t; String(x)
	| (TTrue::t) 		-> lookahead := t; Bool(true)
	| (TFalse::t)		-> lookahead := t; Bool(false)
	| (TLParen::t) 		-> lookahead := t;
		let ast_list = parse_list lookahead in
			(match !lookahead with
			| (TRParen::t) -> lookahead := t; ast_list
			| _ -> raise (Invalid_expression "Expected end of list."))
	| _ -> raise Unknown_token
	

and parse_list lookahead =
	match !lookahead with
	| (TRParen::t) 	-> List []
	| [] 			-> List []
	| _ 			->
		let sexpr = [(parse_sexpr lookahead)] in
		let slist = (parse_list lookahead) in
			(match slist with
			| (List(ast_list)) -> List (sexpr @ ast_list)
			| _ -> raise (Invalid_expression "Unexpected error."))
				
let parse l =
	let lookahead = ref l in
	let ast = parse_sexpr lookahead in
	if !lookahead = [] then
		ast
	else
		raise (Invalid_expression "Unexpected end of parsing.");;

(* HINT:  If you need to define mutually recursive functions, use "and", e.g.

  let rec parse_list <params> = <body>

  and parse_sexpr <params> = <body>

*)

type value =
    Val_Num of int
  | Val_Bool of bool
  | Val_String of string
  | Val_Nil
  | Val_Cons of value * value
  (* You may extend this type, but do not change any of the constructors
     we have given you *)
  (* Extending to represent closure *)
  | Closure of (value -> value) * ((ast * value) list)

(* The following function may come in handy *)
let rec string_of_value = function
    Val_Num n -> string_of_int n
  | Val_Bool true -> "#t"
  | Val_Bool false -> "#f"
  | Val_String s -> "\"" ^ s ^ "\""
  | Val_Nil -> "nil"
  | Val_Cons (v1, v2) -> "(cons " ^ (string_of_value v1) ^ " " ^
      (string_of_value v2) ^ ")"
  | Closure(_,_) -> "<cl>"

(************************************************************************)

(* Write your evaluator here *)
exception Not_implemented;;
exception Unknown_AST;;
exception Invalid_parameter;;

let top_level = ref [(Id "nil", Val_Nil)];;

let get_value env ast = 
	(* Try looking in the local environment *)
	try List.assoc ast env with Not_found ->
	(* else try the top level environment *)
	try List.assoc ast !top_level with Not_found ->
		print_endline("% " ^ unparse ast); raise Invalid_parameter;;

let rec eval_local env ast = 
	match ast with
	| Id(x)		-> get_value env ast
	| Num(x) 	-> Val_Num x
	| Bool(x)	-> Val_Bool x
	| String(x)	-> Val_String x
	| List(x)	-> 
		match x with
		| [] -> raise Invalid_parameter
		| Id("lambda")::List([parameter])::body::[] -> 
			create_closure env parameter body
		| Id("define")::parameter::value::[] ->
			let result = eval_local env value in
			top_level := (parameter,result) :: !top_level; Val_Nil
		| Id("if")::operands -> if_operator env operands
		| Id("+")::operands	-> plus_operator env operands
		| Id("-")::operands	-> minus_operator env operands
		| Id("*")::operands	-> multiply_operator env operands
		| Id("=")::operands	-> equals_operator env operands
		| (Id("cons")::operands) -> cons_operator env operands
		| (Id("car")::operand::[]) -> car_operator env operand
		| (Id("cdr")::operand::[]) -> cdr_operator env operand
		| Id("boolean?")::operand::[] -> boolean_operator env operand
		| Id("number?")::operand::[] -> number_operator env operand
		| Id("string?")::operand::[] -> string_operator env operand
		| Id("pair?")::operand::[] -> pair_operator env operand
		| func::operand::[] -> 
			(match eval_local env func with
			| Closure(cl_fun, cl_env) -> cl_fun(eval_local env operand)
			| _ -> raise Invalid_parameter)
		| [element] -> eval_local env element
		| _	 -> raise Invalid_parameter

and create_closure env parameter body =
	Closure((function x -> eval_local((parameter,x)::env) body), env)

and if_operator env operands = 
	match operands with
	| cond::tr::[] -> 
		(match eval_local env cond with
		| Val_Bool(x) -> if x then (eval_local env tr) else Val_Nil
		(* anything else is equivalent to true *)
		| _ -> eval_local env tr)
	| cond::tr::fl::[] ->
		(match eval_local env cond with
		| Val_Bool(x) -> if x then (eval_local env tr) else (eval_local env fl)
		(* anything else is equivalent to true *)
		| _ -> eval_local env tr)
	| _ -> raise(Invalid_parameter)

and plus_operator env operands = 
	match operands with
	| []	-> Val_Num(0)
	| [h]	-> eval_local env h
	| h::t	-> 
		match eval_local env h with
		| Val_Num(x) -> 
			(match plus_operator env t with
			| Val_Num(y) -> Val_Num(x + y)
			| _	-> raise Invalid_parameter)
		| _	-> raise Invalid_parameter

and minus_operator env operands = 
	match operands with
	| []	-> raise Invalid_parameter
	| [h]	-> minus_operator env ([Num(0);h])
	| h::t	-> 
		match eval_local env h with
		| Val_Num(x) -> 
			(match plus_operator env t with
			| Val_Num(y) -> Val_Num(x - y)
			| _ -> raise Invalid_parameter)
		| _ -> raise Invalid_parameter

and multiply_operator env operands = 
	match operands with
	| []	-> Val_Num(1)
	| [h]	-> eval_local env h
	| h::t	-> 
		match eval_local env h with
		| Val_Num(x) -> 
			(match multiply_operator env t with
			| Val_Num(y) -> Val_Num(x * y)
			| _ -> raise Invalid_parameter)
		| _ -> raise Invalid_parameter

and equals_operator env operands = 
	match operands with
	| []	-> Val_Bool(true)
	| [h]	-> Val_Bool(true)
	| h::t	-> 
		match eval_local env h with
		| Val_Num(x) -> 
			let equals_x y = (Val_Num(x) = y) in
			Val_Bool(List.for_all equals_x (List.map (eval_local env) t))
		| _ -> raise Invalid_parameter

and cons_operator env operands = 
	match operands with
	| (x::y::[]) ->
		let value_x = eval_local env x in
		let value_y = eval_local env y in
		Val_Cons(value_x, value_y)
	| _ -> raise Invalid_parameter
	
and car_operator env operand =
	match eval_local env operand with
	| Val_Cons(car, cdr) -> car
	| _ -> raise Invalid_parameter

and cdr_operator env operand =
	match eval_local env operand with
	| Val_Cons(car, cdr) -> cdr
	| _ -> raise Invalid_parameter

and boolean_operator env operand = 
	match eval_local env operand with
	| Val_Bool(x) 	-> Val_Bool(true)
	| _ 			-> Val_Bool(false)

and number_operator env operand = 
	match eval_local env operand with
	| Val_Num(x) 	-> Val_Bool(true)
	| _ 			-> Val_Bool(false)

and string_operator env operand = 
	match eval_local env operand with
	| Val_String(x)	-> Val_Bool(true)
	| _ 			-> Val_Bool(false)

and pair_operator env operand = 
	match eval_local env operand with
	| Val_Cons(x,y) -> Val_Bool(true)
	| _ 			-> Val_Bool(false)

let eval ast = eval_local [] ast;;

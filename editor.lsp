;; this is the Lisp side of the editor

(do
 (= char-forward
    (fn ()
        (point-move 1)))
 
 (= char-backward
    (fn ()
        (point-move -1)))

  (= word-forward
     (fn ()
         (find-first-not-in-forward ",._ ")
         (find-first-in-forward ",._ ")
         ))
 
  (= word-backward
     (fn ()
         (find-first-not-in-backward ",._ ")
         (find-first-in-backward ",._ ")))
 
 (= keybindings
    (alist "a" t
           "b" t
           "c" t
           "d" t
           "e" t
           "f" t
           "g" t
           "h" t
           "i" t
           "j" t
           "k" t
           "l" t
           "m" t
           "n" t
           "o" t
           "p" t
           "q" t
           "r" t
           "s" t
           "t" t
           "u" t
           "v" t
           "w" t
           "x" t
           "y" t
           "z" t
           "1" t
           "2" t
           "3" t
           "4" t
           "5" t
           "6" t
           "7" t
           "8" t
           "9" t
           "0" t
           "," t
           "." t
           "\\" t
           "/" t
           "(" t
           ")" t
           ";" t
           "-" t
           "=" t
           "'" t
           "A" t
           "B" t
           "C" t
           "D" t
           "E" t
           "F" t
           "G" t
           "H" t
           "I" t
           "J" t
           "K" t
           "L" t
           "M" t
           "N" t
           "O" t
           "P" t
           "Q" t
           "R" t
           "S" t
           "T" t
           "U" t
           "V" t
           "W" t
           "X" t
           "Y" t
           "Z" t
           " " t
           "!" t
           "@" t
           "#" t
           "$" t
           "%" t
           "^" t
           "&" t
           "*" t
           "{" t
           "}" t
           "<" t
           ">" t
           "?" t
           "|" t
           "[" t
           "]" t
           ":" t
           "_" t
           "+" t
           "C-f" char-forward
           "C-b" char-backward
           "M-f" word-forward
           "M-b" word-backward
           ))

 (= shift-letter (fn (key) (upper key)))

  (= shift-number
     (fn (key)
         (nth (- (ord key) 0x30)
              '("}" "!" "@" "#" "$" "%" "^" "&" "*" "{"))))

  (= shift-other
     (fn (key)
         (case key
           "," "<"
           "." ">"
           "/" "?"
           "-" "_"
           "=" "+"
           "\\" "|"
           "(" "["
           ")" "]"
           ";" ":")))
 
  (= encode-key
     (fn (key modifiers)
         (print "KEY:" key "MODS:" modifiers)
         (when (is (& modifiers 1) 1)
           (if (<= (ord "a") (ord key) (ord "z"))
               (= key (shift-letter key))
               (if (<= (ord "0") (ord key) (ord "9"))
                   (= key (shift-number key))
                   (= key (shift-other key)))))
         (when (is (& modifiers 4) 4)
           (= key (string "M-" key)))
         (when (is (& modifiers 2) 2)
           (= key (string "C-" key)))
         key))

  (= handle-key
     (fn (key modifiers)
         (let (k (encode-key key modifiers))
           (let (func (alref k keybindings))
             (if func
                 (if (is func t)
                     (insert-char k)
                     (apply func))))))))
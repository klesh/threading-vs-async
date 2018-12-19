#!/usr/bin/env fish


function show-text
  clear

  for i in (seq (math "($LINES-6)/2"))
    echo ''
  end

  figlet -ctf banner (echo $argv[1] | tr [a-z] [A-Z]) | lolcat

  read -l -P '' input
  if [ "$input" = "q" ]
    exit 0
  end
end

show-text "Threading VS Async"
show-text "Multi-tasking"
show-text "Multi-threading"
show-text "Functionalities"
show-text "Performance"
show-text "Limitation"
show-text "I/O bound to CPU bound"
show-text "Pros & Cons"

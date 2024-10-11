\NeedsTeXFormat{LaTeX2e}
\ProvidesClass{hw}
\LoadClass[12pt, a4paper, draft]{article}

\usepackage{times}
\usepackage[top=1in, bottom=1in, left=.7in, right=1in]{geometry}

% \usepackage{microtype}
\sloppy

\usepackage{enumitem}
% \setlist[enumerate]{topsep=0.5em, partopsep=0em, parsep=0.5em, itemsep=.3cm, left=1em}
% topsep: Space before the list begins.
% partopsep: Space added when the list starts in a new paragraph.
% parsep: Space between paragraphs inside list items.
% itemsep: Space between individual list items.

\usepackage{listings}
\usepackage{xcolor}  % Optional: if you want to customize colors
\definecolor{lightgray}{rgb}{0.95,0.95,0.95}

\lstset{
    language=C,        % Choose the language
    basicstyle=\ttfamily\color{violet},   % Code font
    keywordstyle=\textbf{\color{purple}},   % Style for keywords
    commentstyle=\color{gray},   % Style for comments
    stringstyle=\color{teal},     % Style for strings
    numbers=left,              % Line numbers on the left
    numberstyle=\tiny\color{gray}, % Style for line numbers
    frame=none,             % Frame around the code
    breaklines=true,          % Auto line breaking
    backgroundcolor=\color{lightgray},
    numbersep=-.5cm,           % dist bw lines and text
    showstringspaces=false
}


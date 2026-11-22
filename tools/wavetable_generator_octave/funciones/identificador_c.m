function id = identificador_c(texto)
% IDENTIFICADOR_C Convierte un texto en un identificador valido de C.
%   '#' -> 's' (sostenido) y cualquier otro caracter no alfanumerico -> '_'.
%
%   identificador_c('D#4v1')  ->  'Ds4v1'
%   identificador_c('F#6v2')  ->  'Fs6v2'

    id = strrep(texto, '#', 's');
    id = regexprep(id, '[^A-Za-z0-9_]', '_');
end

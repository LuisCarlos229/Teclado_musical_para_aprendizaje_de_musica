function f0_teorica = calcular_f0_teorica_desde_nombre(nombre_base)
% CALCULAR_F0_TEORICA_DESDE_NOMBRE Extrae la frecuencia fundamental teórica
% a partir del nombre de un archivo de audio.
%
%   Uso: f0_teorica = calcular_f0_teorica_desde_nombre('F#5v1')
%
%   Entrada:
%       nombre_base - String con el nombre del archivo (ej. 'A1v1', 'C#4')
%   Salida:
%       f0_teorica  - Frecuencia teórica en Hz (double).

    % Arreglo de notas en orden cromático estándar (C es el índice 0)
    notas_str = {'C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'};

    % Verificamos si el nombre contiene un sostenido
    tiene_sostenido = ~isempty(strfind(nombre_base, '#'));

    % Extraemos la letra de la nota y el número de octava
    if tiene_sostenido
        letra_nota = nombre_base(1:2);
        octava_nota = str2double(nombre_base(3));
    else
        letra_nota = nombre_base(1);
        octava_nota = str2double(nombre_base(2));
    end

    % Buscamos el índice cromático de la nota (0 a 11)
    idx_nota_midi = 0;
    for idx_n = 1:length(notas_str)
        if strcmp(notas_str{idx_n}, letra_nota)
            idx_nota_midi = idx_n - 1;
            break;
        end
    end

    % Cálculo del número de nota MIDI estándar
    % (La octava de afinación C-1 corresponde a MIDI 0. Por ende, sumamos 1 a la octava)
    midi_note = (octava_nota + 1) * 12 + idx_nota_midi;

    % Cálculo de la frecuencia teórica (A4 = 440 Hz = MIDI 69)
    f0_teorica = 440 * 2^((midi_note - 69) / 12);

end

function generar_libreria_maestra(dir_notas, config)
% GENERAR_LIBRERIA_MAESTRA Lee los .h individuales DE UN INSTRUMENTO
% (<instrumento>_<nota>.h), los ordena por frecuencia fundamental y los
% empaqueta en un unico header estructurado para el motor de sintesis.
%
%   Salida: salida_h/<instrumento>.h  con
%     #define <INSTRUMENTO>_CANTIDAD_ZONAS  N
%     #define <INSTRUMENTO>_ENV_LENGTH      config.N_puntos_envolvente
%     const ZonaWavetable_t banco_<instrumento>[N]
%
%   Cada entrada del banco toma la duracion de su nota de la macro
%   <INSTRUMENTO>_<NOTA>_DURACION_S escrita por exportar_cabeceras_c. Si un
%   .h de nota es anterior a esa version y no la tiene, la macro se define en
%   0.0f y el firmware usa la duracion por defecto del instrumento.
%
%   Solo toma archivos que empiezan con '<instrumento>_'; el maestro
%   ('<instrumento>.h') y las notas de otros instrumentos quedan fuera.

    instrumento = config.nombre_instrumento;
    macro       = upper(instrumento);

    carpeta_salida_h = fullfile(dir_notas, 'salida_h');
    ruta_maestra     = fullfile(carpeta_salida_h, [instrumento, '.h']);

    archivos = dir(fullfile(carpeta_salida_h, [instrumento, '_*.h']));
    if isempty(archivos)
        warning('No se encontraron archivos .h de "%s" para unificar.', instrumento);
        return;
    end

    fprintf('\n---> [EMPAQUETADO] Extrayendo datos de %d notas de "%s"...\n', numel(archivos), instrumento);

    notas_base_str = {'C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'};
    entradas = struct('archivo', {}, 'simbolo', {}, 'midi', {}, 'freq', {});

    for i = 1:numel(archivos)
        archivo = archivos(i).name;
        simbolo = archivo(1:end-2);                          % flauta_Ds4v1
        id_nota = simbolo(length(instrumento) + 2 : end);    % Ds4v1

        % Letra, sostenido opcional ('s') y octava
        tok = regexp(id_nota, '^([A-G])(s?)(\d)', 'tokens', 'once');
        if isempty(tok)
            warning('generar_libreria_maestra: no pude interpretar la nota de "%s", se omite.', archivo);
            continue;
        end
        nota_str = tok{1};
        if ~isempty(tok{2}), nota_str = [nota_str, '#']; end

        idx_nota = find(strcmp(notas_base_str, nota_str)) - 1;
        midi     = (str2double(tok{3}) + 1) * 12 + idx_nota;

        entradas(end+1) = struct('archivo', archivo, 'simbolo', simbolo, ...
                                 'midi', midi, 'freq', 440.0 * 2^((midi - 69) / 12));
    end

    n = numel(entradas);
    if n == 0
        warning('No hubo notas validas para unificar.');
        return;
    end

    % Ordenamiento de grave a agudo (estable: ante igual nota, mantiene v1, v2...)
    [midis_ord, orden] = sort([entradas.midi]);
    entradas = entradas(orden);
    if any(diff(midis_ord) == 0)
        warning('Hay mas de una version de la misma nota; el banco incluye todas.');
    end

    % Escritura del Archivo Maestro
    fileID = fopen(ruta_maestra, 'w');
    if fileID == -1, error('No se pudo crear el archivo maestro.'); end

    fprintf(fileID, '#ifndef %s_H\n', macro);
    fprintf(fileID, '#define %s_H\n\n', macro);
    fprintf(fileID, '#include <stdint.h>\n\n');

    fprintf(fileID, '// === ESTRUCTURA DE DATOS DEL MOTOR DE SINTESIS (compartida entre instrumentos) ===\n');
    fprintf(fileID, '#ifndef ZONA_WAVETABLE_T_DEFINED\n');
    fprintf(fileID, '#define ZONA_WAVETABLE_T_DEFINED\n');
    fprintf(fileID, 'typedef struct {\n');
    fprintf(fileID, '    const int16_t* wavetable;\n');
    fprintf(fileID, '    const uint16_t* envolvente;\n');
    fprintf(fileID, '    float frecuencia_base;\n');
    fprintf(fileID, '    uint16_t indice_tecla;\n');
    fprintf(fileID, '    float duracion_s;          // tiempo que abarca la envolvente (0 = usar valor por defecto)\n');
    fprintf(fileID, '} ZonaWavetable_t;\n');
    fprintf(fileID, '#endif\n\n');

    fprintf(fileID, '// === INCLUDES DE NOTAS INDIVIDUALES ===\n');
    for i = 1:n
        fprintf(fileID, '#include "%s"\n', entradas(i).archivo);
    end
    fprintf(fileID, '\n');

    fprintf(fileID, '// === BANCO DE DATOS ORDENADO (De Grave a Agudo) ===\n');
    fprintf(fileID, '#define %s_CANTIDAD_ZONAS %d\n', macro, n);
    fprintf(fileID, '// Puntos de envolvente de cada nota (se recorren en duracion_s segundos)\n');
    fprintf(fileID, '#define %s_ENV_LENGTH %d\n\n', macro, config.N_puntos_envolvente);

    % Respaldo para .h de notas sin la macro de duracion
    fprintf(fileID, '// Respaldo: .h de notas generados sin duracion -> 0.0f (valor por defecto en el firmware)\n');
    for i = 1:n
        macro_dur = [upper(entradas(i).simbolo), '_DURACION_S'];
        fprintf(fileID, '#ifndef %s\n#define %s 0.0f\n#endif\n', macro_dur, macro_dur);
    end
    fprintf(fileID, '\n');

    fprintf(fileID, 'const ZonaWavetable_t banco_%s[%s_CANTIDAD_ZONAS] = {\n', instrumento, macro);

    for i = 1:n
        valor_tecla = 1 + (i - 1) * 3;    % indice de tecla (convencion del motor)
        fprintf(fileID, '    {%s_wavetable, %s_envelope, %.2ff, %d, %s_DURACION_S}', ...
                entradas(i).simbolo, entradas(i).simbolo, entradas(i).freq, valor_tecla, ...
                upper(entradas(i).simbolo));
        if i < n, fprintf(fileID, ',\n'); else, fprintf(fileID, '\n'); end
    end

    fprintf(fileID, '};\n\n');
    fprintf(fileID, '#endif // %s_H\n', macro);
    fclose(fileID);

    fprintf('---> [EXITO] Libreria maestra "%s.h" generada en salida_h.\n', instrumento);
end

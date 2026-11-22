function exportar_cabeceras_c(wavetable, envolvente, nombre_base, dir_notas, config)
% EXPORTAR_CABECERAS_C Convierte los arreglos flotantes a enteros para I2S
% y escribe la sintaxis de lenguaje C en un archivo header.
%
% Todo lo que se exporta lleva el nombre del instrumento como prefijo
% (config.nombre_instrumento), de modo que instrumentos distintos nunca
% comparten archivo, simbolo, macro ni guarda de inclusion.

    carpeta_salida = fullfile(dir_notas, 'salida_h');
    if ~exist(carpeta_salida, 'dir'), mkdir(carpeta_salida); end

    % Escalamiento a 16 bits
    wavetable_escalada = round(wavetable * 32767);
    wavetable_escalada(wavetable_escalada > 32767) = 32767;
    wavetable_escalada(wavetable_escalada < -32768) = -32768;

    envolvente_uint16 = round(envolvente * 65535);
    envolvente_uint16(envolvente_uint16 > 65535) = 65535;
    envolvente_uint16(envolvente_uint16 < 0) = 0;

    % Nombres seguros para C (sin '#') y con prefijo de instrumento
    prefijo = [config.nombre_instrumento, '_', identificador_c(nombre_base)];   % flauta_Ds4v1
    guarda  = upper(prefijo);                                                    % FLAUTA_DS4V1

    nombre_archivo_h = fullfile(carpeta_salida, [prefijo, '.h']);
    fileID_h = fopen(nombre_archivo_h, 'w');
    if fileID_h == -1, error('No se pudo crear %s', nombre_archivo_h); end

    fprintf(fileID_h, '#ifndef %s_H\n#define %s_H\n\n#include <stdint.h>\n\n', guarda, guarda);

    % Escritura Wavetable
    fprintf(fileID_h, '#define %s_LENGTH %d\n\n', guarda, config.N_WAVETABLE);
    fprintf(fileID_h, 'const int16_t %s_wavetable[%d] = {\n    ', prefijo, config.N_WAVETABLE);
    for j = 1:config.N_WAVETABLE
        fprintf(fileID_h, '%d', wavetable_escalada(j));
        if j < config.N_WAVETABLE, fprintf(fileID_h, ', '); end
        if mod(j, 16) == 0 && j < config.N_WAVETABLE, fprintf(fileID_h, '\n    '); end
    end
    fprintf(fileID_h, '\n};\n\n');

    % Escritura Envolvente
    fprintf(fileID_h, '#define %s_ENV_LENGTH %d\n\n', guarda, config.N_puntos_envolvente);
    fprintf(fileID_h, 'const uint16_t %s_envelope[%d] = {\n    ', prefijo, config.N_puntos_envolvente);
    for j = 1:config.N_puntos_envolvente
        fprintf(fileID_h, '%d', envolvente_uint16(j));
        if j < config.N_puntos_envolvente, fprintf(fileID_h, ', '); end
        if mod(j, 16) == 0 && j < config.N_puntos_envolvente, fprintf(fileID_h, '\n    '); end
    end
    fprintf(fileID_h, '\n};\n\n');

    % Duración real de la nota: tiempo que cubren los N_puntos_envolvente.
    % Es un #define (no un const float) para poder usarlo en el inicializador
    % estático del banco del instrumento.
    fprintf(fileID_h, '// Duracion de la nota: tiempo que abarcan los %d puntos de la envolvente\n', config.N_puntos_envolvente);
    fprintf(fileID_h, '#define %s_DURACION_S %.4ff\n\n', guarda, config.duracion_reconstruccion);
    fprintf(fileID_h, '#endif\n');

    fclose(fileID_h);
end
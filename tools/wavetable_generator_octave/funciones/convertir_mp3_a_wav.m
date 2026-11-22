function convertir_mp3_a_wav(dir_notas)
% CONVERTIR_MP3_A_WAV Escanea archivos .mp3, extrae la nota inteligentemente
% del nombre original (ej. flute_A4_... -> A4v1.wav) y los decodifica.

    archivos_mp3 = dir(fullfile(dir_notas, '*.mp3'));
    num_mp3 = length(archivos_mp3);

    if num_mp3 > 0
        fprintf('\n---> [PRE-PROCESAMIENTO] Se encontraron %d archivos .mp3.\n', num_mp3);

        ruta_original = pwd;
        cd(dir_notas);

        % Crear carpeta para aislar los MP3 una vez convertidos y evitar repeticiones
        carpeta_backup = 'mp3_procesados';
        if ~exist(carpeta_backup, 'dir'), mkdir(carpeta_backup); end

        % Patrón Regex: Busca _ ó inicio, Letra A-G, opcional # ó s, y número 0-9, seguido de _ ó fin
        patron_nota = '(?:_|^)([A-G][#bs]?\d)(?:_|$)';

        for i = 1:num_mp3
            nombre_mp3 = archivos_mp3(i).name;
            [~, nombre_base, ~] = fileparts(nombre_mp3);

            % Buscar la nota dentro del nombre largo usando Regex
            coincidencias = regexp(nombre_base, patron_nota, 'tokens');

            if ~isempty(coincidencias)
                % Extraer la nota (y cambiar la 's' por '#' si la librería usa 'Fs4' en lugar de 'F#4')
                nota_pura = coincidencias{1}{1};
                nota_pura = strrep(nota_pura, 's', '#');

                % Asignar versión secuencial para que no se sobreescriban notas iguales
                version = 1;
                nombre_wav = sprintf('%sv%d.wav', nota_pura, version);
                while exist(nombre_wav, 'file')
                    version = version + 1;
                    nombre_wav = sprintf('%sv%d.wav', nota_pura, version);
                end
            else
                % Fallback: Si el nombre no tiene una nota reconocible, lo deja como estaba
                nombre_wav = [nombre_base, '.wav'];
            end

            fprintf('     [CONVIRTIENDO] %s -> %s ... ', nombre_mp3, nombre_wav);
            try
                [audio, fs] = audioread(nombre_mp3);
                % Convertir y forzar 16-bits para máxima compatibilidad
                audiowrite(nombre_wav, audio, fs, 'BitsPerSample', 16);

                % Mover el MP3 original a la carpeta de procesados
                movefile(nombre_mp3, fullfile(carpeta_backup, nombre_mp3));
                fprintf('OK.\n');
            catch
                fprintf('ERROR (Archivo corrupto o falta codec).\n');
            end
        end

        cd(ruta_original); % Regresamos a la ruta principal
        fprintf('---> [PRE-PROCESAMIENTO] Conversión y renombrado finalizados.\n');
    end
end

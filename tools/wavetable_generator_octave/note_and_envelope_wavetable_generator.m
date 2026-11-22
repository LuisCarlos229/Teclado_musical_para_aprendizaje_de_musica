%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% =============================  INICIO DE CÓDIGO ============================================
%               note and envelope wave table generator
% --------------------------------------------------------------------------------------------
pkg load signal

clc
clear all
close all

disp('===================================================================');
fprintf(' Iniciando Generador Modular de Wavetables \n');
disp('===================================================================');

% =========================================================================
% 1. GESTIÓN DE RUTAS (CARPETAS)
% =========================================================================
% Obtiene la ruta actual donde está este script (raíz del proyecto)
%ruta_raiz = pwd;
%ruta_raiz = 'C:\Users\luisc\Desktop\OneDrive\Documentos\.F.I.U\5° Año. ing. Elec\Proyecto y diseño electronico\Octave\main';
ruta_raiz = fileparts(mfilename('fullpath'));

% Agrega la carpeta 'funciones' al path para que Octave reconozca los módulos
addpath(fullfile(ruta_raiz, 'funciones'));

% Define la carpeta 'notas' como el directorio de lectura/escritura
nombre_instrumento = 'SalamanderGrandPiano';   % solo letras y numeros, sin '_' ni espacios----------------------------------- IMPORTANTE <-----
if isempty(regexp(nombre_instrumento, '^[A-Za-z][A-Za-z0-9]*$', 'once'))
    error('nombre_instrumento "%s" invalido.', nombre_instrumento);
end
dir_notas = fullfile(ruta_raiz, 'notas', nombre_instrumento);
if ~exist(dir_notas, 'dir'), error('No existe la carpeta: %s', dir_notas); end

% =========================================================================
% 2. ESTRUCTURA DE CONFIGURACIÓN (Tuning de Síntesis)
% =========================================================================
config.N_WAVETABLE = 1024;
config.N_puntos_envolvente = 512;
config.duracion_reconstruccion = 4;
config.nivel_resolucion = 3;  % ----------------- CONTROL DE RESOLUCIÓN ---------------------------------
                              % 1 = BÁSICA  (Alta precisión temporal, baja precisión frecuencial)
                              % 2 = BAJA
                              % 3 = MEDIA   (Equilibrio estándar)
                              % 4 = ALTA
                              % 5 = MÁXIMA  (Baja precisión temporal, alta resolución para armónicos exactos)

% Parámetros de Ventana y Detección f0
config.N_CICLOS_VENTANA_ANALISIS = 15;   % Cantidad de ciclos de la onda fundamental requeridos para calcular la FFT (define la longitud temporal de la ventana).
config.N_CICLOS_OFFSET_ATAQUE    = 3;    % Ciclos que el algoritmo deja pasar después del ataque para asegurar que analiza la nota ya estabilizada.
config.DURACION_VENTANA_PISO_S   = 0.05; % Límite mínimo de la ventana (50 ms) para evitar FFTs muy cortas en notas agudas.
config.OFFSET_ATAQUE_PISO_S      = 0.10; % Tiempo mínimo de espera (100 ms) para saltar el violento golpe inicial (transitorio inarmónico) del martillo.
config.TOLERANCIA_BUSQUEDA_F0    = 0.05; % Margen de error base (+/- 5%) al buscar el pico real respecto a la frecuencia teórica (ahora dinámico en notas agudas).

% Parámetros de Armónicos y Fase de Schroeder
config.MARGEN_BUSQUEDA_ARMONICO_PCT = 0.03; % Ventana de búsqueda (+/- 3%) para rastrear armónicos, compensando la inarmonía natural de las cuerdas.
config.MARGEN_NYQUIST               = 0.60; % Freno de seguridad (60% de fs/2 del archivo original) para dejar de extraer armónicos y evitar el aliasing digital.
config.UMBRAL_ARMONICO_dB           = -50;  % "Noise Gate": Los armónicos cuya magnitud esté 50 dB por debajo del pico principal se borran (se hacen 0).
config.N_ARMONICOS_MAX_TABLA        = floor(config.N_WAVETABLE / 2); % Límite absoluto de armónicos que caben en la wavetable sin violar Nyquist.
config.FS_DESTINO                   = 44100; % Frecuencia de muestreo del ESP32 (SAMPLE_RATE_HZ en IO_config.h).
config.MARGEN_NYQUIST_DESTINO       = 0.90;  % Los armónicos de la tabla no superan el 90% de FS_DESTINO/2 en el ESP32...
config.MAX_TRANSPOSICION_ST         = 3;     % ...aun cuando una tecla reutiliza la tabla hasta 3 semitonos más aguda (Instrumentos.h).


% Parámetros de Recorte Inicial (Trigger de Silencio/Ruido)
config.UMBRAL_TRIGGER_PCT  = 0.40; % Dispara cuando la señal supera el 40% de su amplitud máxima
config.PRE_ROLL_TRIGGER_MS = 10;   % Conserva 10 ms previos al trigger para asegurar el transitorio (ataque)
                                   % Para instrumentos de ataque lento (flauta, violin) conviene bajar UMBRAL_TRIGGER_PCT a 0.10-0.15.

% Parámetros de Recorte Final (Cola de ruido / reverberación)
% La duración exportada al ESP32 es la del audio recortado, y los N_puntos_envolvente
% se reparten sobre ella: una cola larga de ruido alarga la nota y le quita resolución al ataque.
config.UMBRAL_COLA_dB   = -60;   % Fin de la nota: último punto donde la envolvente RMS supera este nivel respecto de su pico
config.MARGEN_COLA_S    = 0.05;  % Margen conservado después de ese punto (lo ocupa el fade-out final de la envolvente)
config.DURACION_MIN_S   = 0.20;  % Duración mínima exportada
config.DURACION_MAX_S   = 0;     % Tope opcional de duración en segundos (0 = sin tope)

% Parámetros de DDS (Hardware)
config.BITS_ACUMULADOR_DDS    = 32;       % Resolución del acumulador de fase del ESP32 (32 bits = máxima precisión de afinación).
config.MODO_INTERPOLACION_DDS = 'lineal'; % Define cómo el emulador de Octave lee los valores intermedios de la wavetable para reconstruir el audio.

% Instrumento muestreado
config.nombre_instrumento = nombre_instrumento; % Nombre para el archivo .h maestro

% =========================================================================
% 3. BANDERAS DE CONTROL (Flujo de Ejecución)
% =========================================================================
flags.graficar_onda_original     = true;  % Genera las graficas de las notas originales en el tiempo y los guarda en notas\graficos_onda
flags.graficar_envolvente        = true;  % Genera las graficas de las envolventes de cada nota y los guarda en notas\graficos_envolvente
flags.reconstruir_dds            = true;  % Realiza la reconstruccion del audio a partir de la muestra
flags.reproducir_audio           = false; % Reproduce el audio de la nota original y de la reconstruida
flags.generar_espectrograma      = true;  % Genera las graficas de los espectrogramas y los guarda en notas\espectrogramas
flags.generar_estadisticas       = true;  % Genera un .txt con las estadisticas y lo guarda en notas\estadisticas
flags.graficar_estadisticas      = true;  % Genera las graficas de las estadisticas y los guarda en notas\estadisticas
flags.convertir_mp3_automatico   = true;  % Busca .mp3 y los pasa a .wav antes de iniciar
flags.exportar_headers_c         = true;  % guarda las muestras en un .h para cada nota (nota + envolvente), y los guarda en salida_h
flags.generar_libreria_maestra   = true;  % Une los .h en un solo archivo estructurado, y los guarda en salida_h

% =========================================================================
% 4. INICIO DE PROCESAMIENTO BATCH
% =========================================================================
% Pre-procesamiento de archivos MP3
if flags.convertir_mp3_automatico
    convertir_mp3_a_wav(dir_notas);
end
% Leemos los archivos .wav directamente desde la carpeta 'notas'
archivos_wav = dir(fullfile(dir_notas, '*.wav'));

if isempty(archivos_wav)
    error('No se encontraron archivos .wav en la carpeta "notas".');
end

num_archivos = length(archivos_wav);
fprintf('---> [BATCH] Se encontraron %d archivos de audio para procesar.\n', num_archivos);

% Preparación de estructuras para estadísticas
estadisticas_batch = zeros(num_archivos, 11);   % col. 11 = duración exportada [s]
nombres_notas = cell(num_archivos, 1);

% Inicializar archivo de reporte en la subcarpeta correspondiente
if flags.generar_estadisticas
    carpeta_stats = fullfile(dir_notas, 'estadisticas');
    if ~exist(carpeta_stats, 'dir'), mkdir(carpeta_stats); end
    ruta_reporte = fullfile(carpeta_stats, 'Reporte_Estadistico_Wavetables_V4.txt');
    fileID_reporte = inicializar_reporte(ruta_reporte);
end

% BUCLE PRINCIPAL
for iter = 1:num_archivos
    archivo_entrada = archivos_wav(iter).name;
    ruta_audio = fullfile(dir_notas, archivo_entrada);
    [~, nombre_base, ~] = fileparts(archivo_entrada);
    nombres_notas{iter} = nombre_base;

    fprintf('\n-------------------------------------------------------------------\n');
    fprintf(' Procesando [%d/%d]: %s \n', iter, num_archivos, archivo_entrada);
    fprintf('-------------------------------------------------------------------\n');

    % --- A. LECTURA DE AUDIO ---
    %[audio_in, fs] = audioread(ruta_audio);
    %canal_analisis = audio_in(:, 1); % Extraemos canal mono
    cd(dir_notas);
    [audio_in, fs] = audioread(archivo_entrada);
    cd(ruta_raiz);

    canal_analisis = audio_in(:, 1); % Extraemos canal mono

    % --- PRE-PROCESAMIENTO: RECORTAR SILENCIO MEDIANTE TRIGGER ---
    pico_maximo_audio = max(abs(canal_analisis));
    umbral_absoluto = pico_maximo_audio * config.UMBRAL_TRIGGER_PCT;

    % Encontrar la primera muestra que supera el umbral de disparo
    idx_trigger = find(abs(canal_analisis) > umbral_absoluto, 1, 'first');

    if ~isempty(idx_trigger)
        % Retroceder los milisegundos configurados en el pre-roll
        muestras_pre_roll = round((config.PRE_ROLL_TRIGGER_MS / 1000) * fs);
        idx_inicio_real = max(1, idx_trigger - muestras_pre_roll);

        % Recortar el audio (Elimina todo el silencio previo)
        canal_analisis = canal_analisis(idx_inicio_real:end);
    end

    % --- PRE-PROCESAMIENTO: RECORTAR LA COLA (ruido de fondo / reverberación) ---
    % Envolvente RMS con ventana de 2 períodos de la fundamental teórica (mín. 10 ms)
    f0_teorica = calcular_f0_teorica_desde_nombre(nombre_base);
    N_audio = length(canal_analisis);
    L_rms = min(N_audio, max(round(2 * fs / f0_teorica), round(0.010 * fs)));
    c_acum = cumsum([0; canal_analisis(:) .^ 2]);
    n_idx = (1:N_audio)';
    ia = max(1, n_idx - floor(L_rms / 2));
    ib = min(N_audio, n_idx + ceil(L_rms / 2) - 1);
    env_rms = sqrt(max((c_acum(ib + 1) - c_acum(ia)) ./ (ib - ia + 1), 0));

    umbral_cola = max(env_rms) * 10^(config.UMBRAL_COLA_dB / 20);
    idx_fin = find(env_rms > umbral_cola, 1, 'last');
    if isempty(idx_fin), idx_fin = N_audio; end
    idx_fin = idx_fin + round(config.MARGEN_COLA_S * fs);
    idx_fin = max(idx_fin, round(config.DURACION_MIN_S * fs));
    if config.DURACION_MAX_S > 0
        idx_fin = min(idx_fin, round(config.DURACION_MAX_S * fs));
    end
    idx_fin = min(idx_fin, N_audio);
    canal_analisis = canal_analisis(1:idx_fin);

    % --- DURACIÓN DE LA NOTA ---
    % Es el intervalo exacto que cubren los N_puntos_envolvente: se exporta al
    % .h de la nota para que el ESP32 recorra la envolvente a la misma velocidad.
    config.duracion_reconstruccion = length(canal_analisis) / fs;
    fprintf('     > Duración útil: %.3f s (archivo tras el trigger: %.3f s, umbral de cola %d dB)\n', ...
            config.duracion_reconstruccion, N_audio / fs, config.UMBRAL_COLA_dB);



    if flags.graficar_onda_original
        graficar_onda_original(canal_analisis, nombre_base, dir_notas);
    end

    % --- B. CÁLCULOS TEÓRICOS Y DETECCIÓN DE F0 ---
    f0_real = detectar_fundamental(canal_analisis, fs, f0_teorica, config);

    diff_hz  = f0_real - f0_teorica;
    diff_pct = 100 * diff_hz / f0_teorica;

    % --- C. EXTRACCIÓN DE ESPECTRO Y CREACIÓN DE WAVETABLE ---
    [amplitudes, num_armonicos] = extraer_espectro_armonico(canal_analisis, fs, f0_real, config);
    wavetable_sintetizada = generar_wavetable_schroeder(amplitudes, num_armonicos, config.N_WAVETABLE);

    % --- D. EXTRACCIÓN DE ENVOLVENTE ---
    envolvente = extraer_envolvente_diezmada(canal_analisis, fs, f0_real, config);

    if flags.graficar_envolvente
        graficar_envolvente(envolvente, nombre_base, dir_notas);
    end

    % --- E. RECONSTRUCCIÓN DDS Y MÉTRICAS ---
    if flags.reconstruir_dds
        audio_sintetizado = emular_acumulador_dds(wavetable_sintetizada, envolvente, fs, f0_real, canal_analisis, config);

        if flags.reproducir_audio
            fprintf('     > Reproduciendo ORIGINAL...\n');
            soundsc(canal_analisis, fs);
            pause(length(canal_analisis)/fs + 0.5); % Espera a que termine

            fprintf('     > Reproduciendo SÍNTESIS...\n');
            soundsc(audio_sintetizado, fs);
            pause(config.duracion_reconstruccion + 0.5);
        end

        if flags.generar_estadisticas
            metricas = calcular_metricas_audio(canal_analisis, audio_sintetizado, fs, f0_real, config);
            estadisticas_batch(iter, :) = [f0_teorica, f0_real, diff_hz, diff_pct, metricas, config.duracion_reconstruccion];
            guardar_linea_reporte(fileID_reporte, nombre_base, estadisticas_batch(iter, :));
        end

        if flags.generar_espectrograma
            generar_espectrogramas_comparativos(canal_analisis, audio_sintetizado, fs, f0_real, nombre_base, dir_notas, config);
        end
    end

    % --- F. EXPORTACIÓN PARA FIRMWARE ---
    if flags.exportar_headers_c
        exportar_cabeceras_c(wavetable_sintetizada, envolvente, nombre_base, dir_notas, config);
    end
end

if flags.generar_estadisticas
    fclose(fileID_reporte);
    fclose('all');
    imprimir_resumen_consola(nombres_notas, estadisticas_batch);

    % Guardar gráficas estadísticas si la bandera está en 'true'
    if flags.graficar_estadisticas
        guardar_graficos_estadisticas(nombres_notas, estadisticas_batch, dir_notas);
    end
end

% --- G. EMPAQUETADO FINAL ---
if flags.generar_libreria_maestra && flags.exportar_headers_c
    generar_libreria_maestra(dir_notas, config);
end

disp('%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%');
fprintf('================================  FIN DE CÓDIGO ==============================================\n');
fprintf('               Muchas gracias por usar esta tesis de Grado                     \n');
fprintf('Alumnos:\n');
fprintf('\t Sr. Hubner, Luis Carlosᵃ.\n');
fprintf('\t Sr. Richart, Matiasᵃ.\n');
printf('ᵃ Facultad de Ingeniería, Universidad Nacional de Misiones (UNaM), Oberá, Misiones, Argentina.\n')
printf('\t Juan Manuel de Rosas 325, Oberá, Misiones, Argentina\n')
disp('%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%');

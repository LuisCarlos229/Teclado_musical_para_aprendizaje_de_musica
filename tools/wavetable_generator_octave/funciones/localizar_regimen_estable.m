function [idx_ini, longitud, info] = localizar_regimen_estable(canal, fs, f0_ref, config)
% LOCALIZAR_REGIMEN_ESTABLE Ubica la ventana de análisis más estable de una nota.
% Criterio común para detectar_fundamental y extraer_espectro_armonico, de
% modo que la f0 y las amplitudes armónicas salgan del mismo tramo de señal.
%
%   [idx_ini, longitud, info] = localizar_regimen_estable(canal, fs, f0_ref, config)
%
%   Método:
%     1. Envolvente RMS de 2 períodos (mín. 5 ms): inmune a muestras aisladas.
%     2. Referencia = percentil 98 de la envolvente (un click no la altera).
%     3. Región activa = envolvente > 25 % de la referencia.
%     4. Fin del ataque = primer punto donde la envolvente llega al 75 %,
%        más una guarda (N_CICLOS_OFFSET_ATAQUE ciclos, mín. GUARDA_POST_ATAQUE_S).
%     5. Entre las ventanas candidatas posteriores (con energía >= 50 % de la
%        referencia) se elige la de menor variación de envolvente (std/media).
%
%   Salidas:
%       idx_ini  - Índice de la primera muestra de la ventana.
%       longitud - Cantidad de muestras de la ventana.
%       info     - Estructura con diagnóstico (estable, cv, idx_ataque, env_ref, motivo).

    canal = canal(:);
    N = length(canal);

    % Parámetros internos (GUARDA_POST_ATAQUE_S es opcional en config)
    umbral_activo  = 0.25;
    umbral_ataque  = 0.75;
    energia_minima = 0.50;
    cv_max_estable = 0.50;
    guarda_min_s   = 0.02;
    if isfield(config, 'GUARDA_POST_ATAQUE_S'), guarda_min_s = config.GUARDA_POST_ATAQUE_S; end

    dur_ventana_s = max(config.N_CICLOS_VENTANA_ANALISIS / f0_ref, config.DURACION_VENTANA_PISO_S);
    L_w = min(round(dur_ventana_s * fs), N);

    info = struct('estable', true, 'cv', NaN, 'idx_ataque', 1, 'env_ref', 0, 'motivo', '');

    % 1. Envolvente RMS por suma acumulada (O(N), sin conv)
    L_env = min(N, max(round(2 * fs / f0_ref), round(0.005 * fs)));
    c = cumsum([0; canal .^ 2]);
    n = (1:N)';
    ia = max(1, n - floor(L_env / 2));
    ib = min(N, n + ceil(L_env / 2) - 1);
    env = sqrt(max((c(ib + 1) - c(ia)) ./ (ib - ia + 1), 0));

    % 2. Referencia robusta
    env_ord = sort(env);
    env_ref = env_ord(max(1, round(0.98 * N)));
    info.env_ref = env_ref;
    if env_ref <= 0
        idx_ini = 1; longitud = L_w;
        info.estable = false; info.motivo = 'señal nula';
        warning('localizar_regimen_estable: señal nula, se usa el inicio del archivo.');
        return;
    end

    % 3. Región activa
    activa = find(env > umbral_activo * env_ref);
    idx_act_ini = activa(1);
    idx_act_fin = activa(end);

    % 4. Fin del ataque + guarda
    idx_ataque = idx_act_ini - 1 + find(env(idx_act_ini:end) >= umbral_ataque * env_ref, 1, 'first');
    info.idx_ataque = idx_ataque;
    guarda = round(max(config.N_CICLOS_OFFSET_ATAQUE / f0_ref, guarda_min_s) * fs);

    % 5. Ventanas candidatas
    paso = max(1, round(L_w / 4));
    cand = (idx_ataque + guarda) : paso : (idx_act_fin - L_w + 1);
    if isempty(cand)
        cand = max(1, min(idx_ataque, N - L_w + 1));
        info.estable = false;
        info.motivo = 'nota corta: no hay ventanas posteriores a la guarda';
        warning('localizar_regimen_estable: nota corta, la ventana incluye el ataque.');
    end

    n_c = numel(cand);
    medias = zeros(n_c, 1);
    cvs = zeros(n_c, 1);
    for i = 1:n_c
        seg = env(cand(i) : cand(i) + L_w - 1);
        medias(i) = mean(seg);
        cvs(i) = std(seg) / max(medias(i), eps);
    end

    valido = medias >= energia_minima * env_ref;
    if ~any(valido), valido = true(n_c, 1); end
    cvs(~valido) = Inf;
    [cv_min, j] = min(cvs);

    idx_ini  = cand(j);
    longitud = L_w;
    info.cv  = cv_min;

    if cv_min > cv_max_estable
        info.estable = false;
        info.motivo = 'envolvente muy variable en todas las ventanas candidatas';
        warning('localizar_regimen_estable: no se encontró un tramo estable (cv = %.2f).', cv_min);
    end
end
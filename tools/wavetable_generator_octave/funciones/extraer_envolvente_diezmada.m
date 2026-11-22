function envolvente_150_norm = extraer_envolvente_diezmada(canal_analisis, fs, f0_real, config)
% EXTRAER_ENVOLVENTE_DIEZMADA (Versión Hilbert)
% Extrae la envolvente analítica de la nota utilizando la Transformada de 
% Hilbert, con suavizado dinámico adaptado a la frecuencia fundamental,
% y decima la señal mediante PCHIP para el sintetizador.
%
%   Uso: env = extraer_envolvente_diezmada(audio, fs, f0_real, config)

    % 1. Acondicionamiento (Filtro Paso Alto para eliminar DC Offset)
    % Hilbert es muy sensible al nivel de continua (DC). Removemos todo 
    % por debajo de 20 Hz para asegurar una envolvente centrada.
    f_corte_hp = 20; 
    [b_hp, a_hp] = butter(2, f_corte_hp / (fs / 2), 'high');
    canal_acondicionado = filtfilt(b_hp, a_hp, canal_analisis);

    % 2. Transformada de Hilbert (Envolvente Analítica)
    % Calcula la magnitud del vector complejo analítico. A diferencia de 
    % la rectificación, esto representa la energía instantánea real.
    envolvente_bruta = abs(hilbert(canal_acondicionado));

    % 3. Suavizado Dinámico basado en f0_real (Eliminación de Ripple)
    % Señales con muchos armónicos generan un "rizado" en la envolvente de Hilbert.
    % Usamos un filtro de media móvil donde el tamaño de la ventana es 
    % EXACTAMENTE igual a un período de la frecuencia fundamental detectada.
    muestras_por_periodo = max(1, round(fs / f0_real));
    ventana_suavizado = ones(muestras_por_periodo, 1) / muestras_por_periodo;
    
    % filtfilt aplica la media móvil en ambas direcciones (fase cero), 
    % cancelando el ripple sin retrasar el transitorio de ataque de la nota.
    envolvente_suavizada = filtfilt(ventana_suavizado, 1, envolvente_bruta);
    
    % Recorte de seguridad para limpiar valores negativos residuales (por cálculo flotante)
    envolvente_suavizada(envolvente_suavizada < 0) = 0;

    % 4. Submuestreo (Decimación) usando interpolación Hermite Cúbica PCHIP
    % Se mantiene idéntico para asegurar la cantidad correcta de puntos en la tabla.
    indices_originales = linspace(1, length(envolvente_suavizada), length(envolvente_suavizada));
    indices_remuestreados = linspace(1, length(envolvente_suavizada), config.N_puntos_envolvente);

    % PCHIP evita la sobrecompensación ("ringing") clásica de los splines
    envolvente_150_norm = interp1(indices_originales, envolvente_suavizada, indices_remuestreados, 'pchip')(:);

    % 5. Normalización al rango [0, 1]
    max_env = max(envolvente_150_norm);
    if max_env > 0
        envolvente_150_norm = envolvente_150_norm / max_env;
    end

    % 6. FADE-OUT SUAVE (RELEASE FORZADO AL FINAL)
    % Evita chasquidos ("cola erizada") forzando que el último tramo de la 
    % envolvente caiga suavemente a cero (función coseno).
    
    porcentaje_fade = 0.05; % El último 5% de la nota será la atenuación
    puntos_totales = length(envolvente_150_norm);
    puntos_fade = max(1, round(puntos_totales * porcentaje_fade));
    
    % Creamos una curva suave que va de 1 a 0
    % Se usa media onda cosenoidal para que el corte no sea lineal/brusco
    x_fade = linspace(0, pi/2, puntos_fade)';
    curva_atenuacion = cos(x_fade);
    
    % Aplicamos la atenuación a los últimos puntos del arreglo
    idx_inicio_fade = puntos_totales - puntos_fade + 1;
    envolvente_150_norm(idx_inicio_fade:end) = envolvente_150_norm(idx_inicio_fade:end) .* curva_atenuacion;
    
    % Forzar el último punto estrictamente a 0 para apagar el DAC
    envolvente_150_norm(end) = 0;

end
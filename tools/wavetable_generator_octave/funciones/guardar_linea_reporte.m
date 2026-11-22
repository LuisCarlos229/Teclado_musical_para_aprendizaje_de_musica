function guardar_linea_reporte(fileID, nombre, stats)
    fprintf(fileID, '%-12s %-11.3f %-11.3f %-9.3f %-8.2f %-9.4f %-9.4f %-11.4f %-9.4f %-11.4f %-9.4f %-8.3f\n', ...
        nombre, stats(1), stats(2), stats(3), stats(4), stats(5), stats(6), stats(7), stats(8), stats(9), stats(10), stats(11));
end

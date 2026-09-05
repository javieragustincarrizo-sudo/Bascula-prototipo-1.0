import os
import gzip
import shutil

Import("env")

def compress_data_folder(source, target, env):
    # Obtiene de forma segura la ruta real de la carpeta data de tu proyecto
    data_dir = env.subst("$PROJECT_DATA_DIR")
    print(f"--> [GZIP SCRIPT] Comprimiendo archivos Web en: {data_dir}")
    
    # Extensiones de archivos de texto que tu servidor web busca comprimidos
    extensions_to_compress = (".html", ".css", ".js", ".json")

    # Escanea toda la estructura de la carpeta data
    for root, dirs, files in os.walk(data_dir):
        for file in files:
            file_path = os.path.join(root, file)
            
            # Filtra solo los archivos editables de texto
            if file_path.endswith(extensions_to_compress):
                gz_path = file_path + ".gz"
                
                # Comprime si el .gz no existe o si modificaste el archivo original recientemente
                if not os.path.exists(gz_path) or os.path.getmtime(file_path) > os.path.getmtime(gz_path):
                    print(f"    Compresion automatica: {file} -> {file}.gz")
                    with open(file_path, "rb") as f_in:
                        with gzip.open(gz_path, "wb", compresslevel=9) as f_out:
                            shutil.copyfileobj(f_in, f_out)

# Vincula el script para que actúe justo antes de generar la imagen del sistema de archivos
env.AddPreAction("$BUILD_DIR/littlefs.bin", compress_data_folder)

package org.guitargirlresuscitation.memorial;
import java.util.Locale;

/** User-facing transfer text; technical failure details stay in English logs. */
final class SaveTransferText {
    final String export, importSave, folder, permission, warning, done, error, busy;
    private SaveTransferText(String... s) {
        export=s[0]; importSave=s[1]; folder=s[2]; permission=s[3];
        warning=s[4]; done=s[5]; error=s[6]; busy=s[7];
    }
    static SaveTransferText forLocale(Locale l) {
        String lang=l.getLanguage();
        if ("zh".equals(lang)) {
            if ("TW".equals(l.getCountry()) || "HK".equals(l.getCountry())
                    || "MO".equals(l.getCountry()) || "Hant".equals(l.getScript()))
                return new SaveTransferText("匯出資料庫", "匯入資料庫", "選擇 Documents 資料夾", "請授權 Documents 資料夾，存檔會放在其下方的套件名稱/saves。可取消，不影響遊戲。", "將覆蓋目前所有存檔，不會自動備份。確定匯入？", "操作完成", "操作失敗：請檢查權限、空間及存檔版本。", "正在處理存檔，請稍候。");
            return new SaveTransferText("导出数据库", "导入数据库", "选择 Documents 文件夹", "请授权 Documents 文件夹，存档会放在其下方的包名/saves。可以取消，不影响游戏。", "将覆盖当前所有存档，不会自动备份。确定导入？", "操作完成", "操作失败：请检查权限、空间及存档版本。", "正在处理存档，请稍候。");
        }
        if ("ja".equals(lang)) return new SaveTransferText("データベースを出力", "データベースを読み込む", "Documents フォルダーを選択", "Documents へのアクセスを許可してください。package/saves に保存します。キャンセルしても遊べます。", "現在の全セーブを上書きします。自動バックアップは作成しません。続けますか？", "完了", "失敗しました。権限、空き容量、セーブのバージョンを確認してください。", "セーブを処理中です。");
        if ("ko".equals(lang)) return new SaveTransferText("데이터베이스 내보내기", "데이터베이스 가져오기", "Documents 폴더 선택", "Documents 접근을 허용해 주세요. package/saves에 저장합니다. 취소해도 게임은 가능합니다.", "현재 모든 저장 데이터를 덮어씁니다. 자동 백업은 만들지 않습니다. 계속할까요?", "완료", "실패했습니다. 권한, 저장 공간 및 저장 버전을 확인하세요.", "저장 데이터를 처리 중입니다.");
        if ("es".equals(lang)) return new SaveTransferText("Exportar base de datos", "Importar base de datos", "Elegir carpeta Documents", "Autoriza Documents; se usará package/saves. Puedes cancelar y jugar.", "Se sobrescribirán TODAS las partidas actuales, sin copia de seguridad automática. ¿Continuar?", "Completado", "Error: revisa permisos, espacio y versión de la partida.", "Procesando partidas…");
        if ("pt".equals(lang)) return new SaveTransferText("Exportar banco de dados", "Importar banco de dados", "Selecionar pasta Documents", "Autorize Documents; será usada package/saves. Você pode cancelar e jogar.", "TODOS os dados salvos atuais serão substituídos, sem backup automático. Continuar?", "Concluído", "Falha: verifique permissões, espaço e versão do arquivo.", "Processando dados salvos…");
        if ("it".equals(lang)) return new SaveTransferText("Esporta database", "Importa database", "Scegli cartella Documents", "Autorizza Documents; verrà usata package/saves. Puoi annullare e giocare.", "TUTTI i salvataggi attuali verranno sovrascritti, senza backup automatico. Continuare?", "Completato", "Errore: controlla permessi, spazio e versione del salvataggio.", "Elaborazione dei salvataggi…");
        if ("vi".equals(lang)) return new SaveTransferText("Xuất cơ sở dữ liệu", "Nhập cơ sở dữ liệu", "Chọn thư mục Documents", "Cho phép truy cập Documents; dữ liệu lưu trong package/saves. Có thể hủy và chơi tiếp.", "Ghi đè TẤT CẢ dữ liệu lưu hiện tại, không tự sao lưu. Tiếp tục?", "Hoàn tất", "Thất bại: kiểm tra quyền, dung lượng và phiên bản dữ liệu.", "Đang xử lý dữ liệu lưu…");
        if ("id".equals(lang)) return new SaveTransferText("Ekspor basis data", "Impor basis data", "Pilih folder Documents", "Izinkan akses Documents; gunakan package/saves. Anda dapat membatalkan dan bermain.", "SEMUA simpanan saat ini akan ditimpa tanpa cadangan otomatis. Lanjutkan?", "Selesai", "Gagal: periksa izin, ruang, dan versi simpanan.", "Memproses simpanan…");
        if ("th".equals(lang)) return new SaveTransferText("ส่งออกฐานข้อมูล", "นำเข้าฐานข้อมูล", "เลือกโฟลเดอร์ Documents", "อนุญาตเข้าถึง Documents เพื่อบันทึกใน package/saves ยกเลิกแล้วเล่นต่อได้", "จะเขียนทับเซฟปัจจุบันทั้งหมด โดยไม่สำรองอัตโนมัติ ต้องการดำเนินการต่อหรือไม่?", "เสร็จแล้ว", "ล้มเหลว: ตรวจสอบสิทธิ์ พื้นที่ และเวอร์ชันเซฟ", "กำลังจัดการเซฟ…");
        if ("hi".equals(lang)) return new SaveTransferText("डेटाबेस निर्यात", "डेटाबेस आयात", "Documents फ़ोल्डर चुनें", "Documents की अनुमति दें; package/saves में सहेजा जाएगा। रद्द करके भी खेल सकते हैं।", "वर्तमान सभी सेव बदल दिए जाएंगे। कोई स्वचालित बैकअप नहीं बनेगा। जारी रखें?", "पूरा हुआ", "विफल: अनुमति, जगह और सेव संस्करण जाँचें।", "सेव संसाधित हो रहे हैं…");
        return new SaveTransferText("Export database", "Import database", "Choose Documents folder", "Allow access to Documents; saves go under package/saves. Canceling does not prevent playing.", "Overwrite ALL current saves? No automatic backup will be created.", "Completed", "Failed: check permissions, storage space and save version.", "Processing saves…");
    }
}

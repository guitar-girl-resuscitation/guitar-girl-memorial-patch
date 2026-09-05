package org.guitargirlresuscitation.memorial;

import java.util.Locale;

/** Our launcher copy, independent of game assets and of the game's save slots. */
final class LauncherText {
    static final int HOME=0, SAVES=1, ABOUT=2, CHECK=3, WEBSITE=4, AUTO=5,
            CHECKING=6, CURRENT=7, AVAILABLE=8, OFFLINE=9, NO_SOURCE=10,
            MISMATCH=11, UPDATE_NOTE=12, REPOSITORIES=13;
    static String get(int key) {
        Locale locale = Locale.getDefault();
        String lang = locale.getLanguage();
        String[] text;
        switch (lang) {
            case "zh":
                boolean traditional = locale.getScript().equalsIgnoreCase("Hant") || locale.getCountry().equals("TW") || locale.getCountry().equals("HK");
                text = traditional
                    ? new String[]{"首頁","存檔","關於與更新","檢查更新","開啟原補丁網站","啟動時檢查更新","正在檢查…","目前已是最新版本","有新版本可用","無法檢查更新，仍可離線遊玩","此安裝包未設定更新來源","更新來源的包名或簽名不符，已拒絕","請回原網站重新製作安裝包並覆蓋安裝。不要卸載或清除資料。","專案倉庫"}
                    : new String[]{"首页","存档","关于与更新","检查更新","打开原补丁网站","启动时检查更新","正在检查…","当前已是最新版本","有新版本可用","无法检查更新，仍可离线游玩","此安装包未设置更新来源","更新来源的包名或签名不符，已拒绝","请回原网站重新制作安装包并覆盖安装。不要卸载或清除数据。","项目仓库"}; break;
            case "ja": text = new String[]{"ホーム","セーブ","情報・更新","更新を確認","元のパッチサイトを開く","起動時に更新を確認","確認中…","最新版です","新しいバージョンがあります","更新を確認できません。オフラインで遊べます","更新元が設定されていません","パッケージまたは署名が一致しません","元のサイトで再パッチし、上書きインストールしてください。アンインストールやデータ削除は不要です。","リポジトリ"}; break;
            case "ko": text = new String[]{"홈","저장","정보 및 업데이트","업데이트 확인","원래 패치 사이트 열기","시작 시 업데이트 확인","확인 중…","최신 버전입니다","새 버전이 있습니다","업데이트 확인 실패. 오프라인 플레이 가능","업데이트 출처가 없습니다","패키지 또는 서명이 일치하지 않습니다","원래 사이트에서 다시 패치한 뒤 덮어 설치하세요. 앱이나 데이터를 삭제하지 마세요.","저장소"}; break;
            case "vi": text = new String[]{"Trang chủ","Bản lưu","Thông tin và cập nhật","Kiểm tra cập nhật","Mở trang vá gốc","Kiểm tra khi khởi động","Đang kiểm tra…","Đã là bản mới nhất","Có phiên bản mới","Không thể kiểm tra. Vẫn chơi ngoại tuyến được","Chưa đặt nguồn cập nhật","Gói hoặc chữ ký không khớp","Vá lại tại trang gốc rồi cài đè. Không gỡ ứng dụng hoặc xóa dữ liệu.","Kho mã nguồn"}; break;
            case "es": text = new String[]{"Inicio","Partidas","Información y actualizaciones","Buscar actualizaciones","Abrir sitio de parche original","Buscar al iniciar","Buscando…","Versión actualizada","Hay una nueva versión","No se pudo comprobar. Puedes jugar sin conexión","Sin origen de actualización","El paquete o la firma no coinciden","Vuelve a parchear en el sitio original e instala encima. No desinstales ni borres datos.","Repositorios"}; break;
            case "it": text = new String[]{"Home","Salvataggi","Info e aggiornamenti","Controlla aggiornamenti","Apri il sito originale","Controlla all'avvio","Controllo…","Versione aggiornata","Nuova versione disponibile","Verifica non riuscita. Puoi giocare offline","Nessuna origine degli aggiornamenti","Pacchetto o firma non corrispondenti","Applica la patch sul sito originale e installa sopra. Non disinstallare o cancellare i dati.","Repository"}; break;
            case "in": case "id": text = new String[]{"Beranda","Simpanan","Info dan pembaruan","Periksa pembaruan","Buka situs patch asal","Periksa saat mulai","Memeriksa…","Versi terbaru","Versi baru tersedia","Gagal memeriksa. Tetap dapat bermain offline","Sumber pembaruan belum diatur","Paket atau tanda tangan tidak cocok","Patch ulang di situs asal lalu pasang sebagai pembaruan. Jangan hapus aplikasi atau data.","Repositori"}; break;
            case "th": text = new String[]{"หน้าหลัก","บันทึก","เกี่ยวกับและอัปเดต","ตรวจสอบอัปเดต","เปิดเว็บไซต์แพตช์เดิม","ตรวจสอบเมื่อเริ่ม","กำลังตรวจสอบ…","เป็นเวอร์ชันล่าสุดแล้ว","มีเวอร์ชันใหม่","ตรวจสอบไม่ได้ แต่ยังเล่นออฟไลน์ได้","ไม่ได้กำหนดแหล่งอัปเดต","แพ็กเกจหรือลายเซ็นไม่ตรงกัน","แพตช์ใหม่จากเว็บไซต์เดิมและติดตั้งทับ อย่าถอนการติดตั้งหรือล้างข้อมูล","คลังซอร์สโค้ด"}; break;
            case "pt": text = new String[]{"Início","Dados salvos","Sobre e atualizações","Verificar atualizações","Abrir site original","Verificar ao iniciar","Verificando…","Versão atualizada","Nova versão disponível","Não foi possível verificar. Pode jogar offline","Sem origem de atualização","Pacote ou assinatura incompatível","Refaça o patch no site original e instale por cima. Não desinstale nem apague os dados.","Repositórios"}; break;
            case "hi": text = new String[]{"होम","सेव","जानकारी और अपडेट","अपडेट जाँचें","मूल पैच वेबसाइट खोलें","शुरू होने पर जाँचें","जाँच जारी…","नवीनतम संस्करण है","नया संस्करण उपलब्ध है","जाँच विफल। ऑफ़लाइन खेल सकते हैं","अपडेट स्रोत तय नहीं है","पैकेज या हस्ताक्षर मेल नहीं खाते","मूल वेबसाइट पर फिर पैच करके ऊपर से इंस्टॉल करें। ऐप या डेटा न मिटाएँ।","रिपॉज़िटरी"}; break;
            default: text = new String[]{"Home","Saves","About & updates","Check for updates","Open original patch website","Check on startup","Checking…","Up to date","New version available","Cannot check; offline play is still available","This build has no update source","Update package or signer mismatch; rejected","Re-patch at the original website and install as an update. Do not uninstall or clear data.","Repositories"};
        }
        return text[key];
    }
    private LauncherText() {}
}

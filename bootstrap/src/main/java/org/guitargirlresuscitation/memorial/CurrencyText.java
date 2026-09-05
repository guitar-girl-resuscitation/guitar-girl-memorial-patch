package org.guitargirlresuscitation.memorial;

/** Resource-free strings; key order is shared with the native currency page. */
final class CurrencyText {
    static final String[] KEYS = {"ch1_like", "ch2_like", "ch2_note", "candy", "chocolate", "fans"};
    final String title, back, close, send, multiplier, count, invalid;
    final String[] names;

    private CurrencyText(String title, String back, String close, String send,
                         String names, String multiplier, String count, String invalid) {
        this.title = title; this.back = back; this.close = close; this.send = send;
        this.names = names.split("\\|", -1); this.multiplier = multiplier;
        this.count = count; this.invalid = invalid;
    }

    static int index(String key) {
        for (int i = 0; i < KEYS.length; i++) if (KEYS[i].equals(key)) return i;
        return -1;
    }

    static boolean isMultiplier(String key) {
        int i = index(key);
        return i == 0 || i == 1 || i == 2 || i == 5;
    }

    static boolean validSyntax(String key, String input) {
        if (index(key) < 0 || input == null || input.length() > 64) return false;
        if (isMultiplier(key)) return input.matches("[0-9]+")
                && input.matches(".*[1-9].*");
        try {
            if (!input.matches("[0-9]+")) return false;
            long count = Long.parseLong(input);
            return count > 0 && count <= Integer.MAX_VALUE;
        } catch (NumberFormatException error) { return false; }
    }

    static CurrencyText forLocale(String locale) {
        String key = locale == null ? "en" : locale.replace('-', '_').toLowerCase(java.util.Locale.ROOT);
        if (key.equals("zh_hans") || key.equals("zh_cn") || key.equals("zh_sg")) key = "zh_chs";
        else if (key.equals("zh_hant") || key.equals("zh_tw") || key.equals("zh_hk")) key = "zh_cht";
        else if (!key.startsWith("zh_") && key.contains("_")) key = key.split("_", 2)[0];
        switch (key) {
            case "zh_chs": return new CurrencyText("补充货币", "返回", "关闭", "发送邮件",
                "CH1 赞|CH2 赞|CH2 音符|糖果|巧克力|CH1 粉丝",
                "输入倍率，按领取时的当前获取基数计算。保留服装、吉他等原有加成；粉丝最终取整。",
                "输入实际数量（正整数），不是倍率。", "请输入有效的正数；糖果和巧克力必须为整数。");
            case "zh_cht": return new CurrencyText("補充貨幣", "返回", "關閉", "寄送郵件",
                "CH1 讚|CH2 讚|CH2 音符|糖果|巧克力|CH1 粉絲",
                "輸入倍率，依領取時的獲取基數計算。保留服裝、吉他等原有加成；粉絲最後取整。",
                "輸入實際數量（正整數），不是倍率。", "請輸入有效正數；糖果與巧克力必須為整數。");
            case "ko": return new CurrencyText("재화 보충", "뒤로", "닫기", "우편 보내기",
                "CH1 좋아요|CH2 하트|CH2 음표|사탕|초콜릿|CH1 팬",
                "수령 시 획득 기준에 적용할 배율입니다. 의상·기타 등 기존 보너스는 유지되며 팬은 정수로 계산됩니다.",
                "실제 개수(양의 정수)를 입력하세요. 배율이 아닙니다.", "올바른 양수를 입력하세요. 사탕과 초콜릿은 정수여야 합니다.");
            case "ja": case "jp": return new CurrencyText("通貨の補充", "戻る", "閉じる", "メール送信",
                "CH1 いいね|CH2 ハート|CH2 音符|キャンディ|チョコ|CH1 ファン",
                "受取時の獲得基準に掛ける倍率です。衣装・ギター等の元のボーナスは維持し、ファン数は整数にします。",
                "実際の個数（正の整数）を入力してください。倍率ではありません。", "有効な正数を入力してください。キャンディとチョコは整数です。");
            case "vi": return new CurrencyText("Bổ sung tài nguyên", "Quay lại", "Đóng", "Gửi thư",
                "CH1 Lượt thích|CH2 Tim|CH2 Nốt nhạc|Kẹo|Sô cô la|CH1 Người hâm mộ",
                "Nhập hệ số theo mức thu nhận lúc nhận thư. Giữ thưởng trang phục, đàn và các thưởng gốc; số fan lấy phần nguyên.",
                "Nhập số lượng thực tế (số nguyên dương), không phải hệ số.", "Nhập số dương hợp lệ. Kẹo và sô cô la phải là số nguyên.");
            case "es": return new CurrencyText("Añadir recursos", "Atrás", "Cerrar", "Enviar correo",
                "CH1 Me gusta|CH2 Corazones|CH2 Notas|Caramelos|Chocolate|CH1 Fans",
                "Multiplicador de la obtención actual al reclamar. Se conservan los bonos originales de ropa y guitarras; los fans se truncan a enteros.",
                "Cantidad real (entero positivo), no multiplicador.", "Introduce un número positivo válido; caramelos y chocolate requieren enteros.");
            case "it": return new CurrencyText("Aggiungi risorse", "Indietro", "Chiudi", "Invia posta",
                "CH1 Mi piace|CH2 Cuori|CH2 Note|Caramelle|Cioccolato|CH1 Fan",
                "Moltiplicatore del guadagno attuale al ritiro. I bonus originali di abiti e chitarre restano; i fan diventano interi.",
                "Quantità effettiva (intero positivo), non moltiplicatore.", "Inserisci un numero positivo valido; caramelle e cioccolato richiedono interi.");
            case "id": return new CurrencyText("Tambah sumber daya", "Kembali", "Tutup", "Kirim surat",
                "CH1 Suka|CH2 Hati|CH2 Not|Permen|Cokelat|CH1 Penggemar",
                "Pengali perolehan saat surat diklaim. Bonus asli pakaian dan gitar tetap berlaku; jumlah penggemar dibulatkan ke bawah.",
                "Jumlah sebenarnya (bilangan bulat positif), bukan pengali.", "Masukkan angka positif yang valid; permen dan cokelat harus bilangan bulat.");
            case "th": return new CurrencyText("เพิ่มทรัพยากร", "กลับ", "ปิด", "ส่งจดหมาย",
                "CH1 ไลก์|CH2 หัวใจ|CH2 โน้ต|ลูกอม|ช็อกโกแลต|CH1 แฟน",
                "ใส่ตัวคูณของอัตรารับในขณะรับจดหมาย โบนัสเดิมจากชุดและกีตาร์ยังคงอยู่ จำนวนแฟนตัดเศษเป็นจำนวนเต็ม",
                "ใส่จำนวนจริงเป็นจำนวนเต็มบวก ไม่ใช่ตัวคูณ", "ใส่จำนวนบวกที่ถูกต้อง ลูกอมและช็อกโกแลตต้องเป็นจำนวนเต็ม");
            case "pt": return new CurrencyText("Adicionar recursos", "Voltar", "Fechar", "Enviar correio",
                "CH1 Curtidas|CH2 Corações|CH2 Notas|Doces|Chocolate|CH1 Fãs",
                "Multiplicador do ganho atual ao resgatar. Mantém os bônus originais de roupas e guitarras; fãs são truncados para inteiros.",
                "Quantidade real (inteiro positivo), não multiplicador.", "Insira um número positivo válido; doces e chocolate exigem inteiros.");
            case "hi": return new CurrencyText("संसाधन जोड़ें", "वापस", "बंद करें", "मेल भेजें",
                "CH1 लाइक|CH2 दिल|CH2 नोट|कैंडी|चॉकलेट|CH1 प्रशंसक",
                "मेल लेते समय की प्राप्ति दर का गुणक दर्ज करें। पोशाक और गिटार के मूल बोनस बने रहेंगे; प्रशंसक संख्या पूर्णांक होगी।",
                "वास्तविक मात्रा (धनात्मक पूर्णांक) दर्ज करें, गुणक नहीं।", "मान्य धनात्मक संख्या दर्ज करें; कैंडी और चॉकलेट के लिए पूर्णांक चाहिए।");
            default: return new CurrencyText("Add currency", "Back", "Close", "Send mail",
                "CH1 Likes|CH2 Hearts|CH2 Notes|Candy|Chocolate|CH1 Fans",
                "Enter a multiplier of the gain basis at claim time. Original outfit, guitar and other bonuses are retained; fans are truncated to an integer.",
                "Enter the actual count (a positive integer), not a multiplier.", "Enter a valid positive value; candy and chocolate require whole numbers.");
        }
    }
}

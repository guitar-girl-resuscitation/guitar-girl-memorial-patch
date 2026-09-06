package org.guitargirlresuscitation.memorial;

import java.util.Locale;

final class DiagnosticText {
    static String get(int key) {
        if (key >= 4) return action(key - 4);
        String lang = Locale.getDefault().getLanguage();
        String[] text;
        switch (lang) {
            case "zh":
                boolean traditional = "Hant".equals(Locale.getDefault().getScript())
                        || "TW".equals(Locale.getDefault().getCountry()) || "HK".equals(Locale.getDefault().getCountry());
                text = traditional ? new String[]{"匯出本次診斷","匯出上次診斷","尚無診斷紀錄","診斷已匯出"}
                        : new String[]{"导出本次诊断","导出上次诊断","暂无诊断记录","诊断已导出"}; break;
            case "ja": text = new String[]{"今回の診断を保存","前回の診断を保存","診断記録はありません","診断を保存しました"}; break;
            case "ko": text = new String[]{"현재 진단 내보내기","이전 진단 내보내기","진단 기록 없음","진단을 내보냈습니다"}; break;
            case "vi": text = new String[]{"Xuất chẩn đoán hiện tại","Xuất chẩn đoán trước","Chưa có chẩn đoán","Đã xuất chẩn đoán"}; break;
            case "es": text = new String[]{"Exportar diagnóstico actual","Exportar diagnóstico anterior","No hay diagnóstico","Diagnóstico exportado"}; break;
            case "it": text = new String[]{"Esporta diagnosi attuale","Esporta diagnosi precedente","Nessuna diagnosi","Diagnosi esportata"}; break;
            case "id": case "in": text = new String[]{"Ekspor diagnosis saat ini","Ekspor diagnosis sebelumnya","Belum ada diagnosis","Diagnosis diekspor"}; break;
            case "th": text = new String[]{"ส่งออกการวินิจฉัยครั้งนี้","ส่งออกการวินิจฉัยครั้งก่อน","ไม่มีข้อมูลวินิจฉัย","ส่งออกแล้ว"}; break;
            case "pt": text = new String[]{"Exportar diagnóstico atual","Exportar diagnóstico anterior","Sem diagnóstico","Diagnóstico exportado"}; break;
            case "hi": text = new String[]{"वर्तमान निदान निर्यात करें","पिछला निदान निर्यात करें","निदान उपलब्ध नहीं","निदान निर्यात किया गया"}; break;
            default: text = new String[]{"Export current diagnostics","Export previous diagnostics","No diagnostics yet","Diagnostics exported"};
        }
        return text[key];
    }
    private static String action(int key) {
        String[] text;
        switch (Locale.getDefault().getLanguage()) {
            case "zh":
                boolean traditional = "Hant".equals(Locale.getDefault().getScript())
                        || "TW".equals(Locale.getDefault().getCountry()) || "HK".equals(Locale.getDefault().getCountry());
                text = traditional ? new String[]{"複製本次診斷","診斷已複製","關閉"}
                        : new String[]{"复制本次诊断","诊断已复制","关闭"}; break;
            case "ja": text = new String[]{"今回の診断をコピー","診断をコピーしました","閉じる"}; break;
            case "ko": text = new String[]{"현재 진단 복사","진단을 복사했습니다","닫기"}; break;
            case "vi": text = new String[]{"Sao chép chẩn đoán hiện tại","Đã sao chép chẩn đoán","Đóng"}; break;
            case "es": text = new String[]{"Copiar diagnóstico actual","Diagnóstico copiado","Cerrar"}; break;
            case "it": text = new String[]{"Copia diagnosi attuale","Diagnosi copiata","Chiudi"}; break;
            case "id": case "in": text = new String[]{"Salin diagnosis saat ini","Diagnosis disalin","Tutup"}; break;
            case "th": text = new String[]{"คัดลอกการวินิจฉัยครั้งนี้","คัดลอกแล้ว","ปิด"}; break;
            case "pt": text = new String[]{"Copiar diagnóstico atual","Diagnóstico copiado","Fechar"}; break;
            case "hi": text = new String[]{"वर्तमान निदान कॉपी करें","निदान कॉपी किया गया","बंद करें"}; break;
            default: text = new String[]{"Copy current diagnostics","Diagnostics copied","Close"};
        }
        return text[key];
    }
    private DiagnosticText() {}
}

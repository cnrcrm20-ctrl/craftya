# Craftya imza anahtarı

| Alan | Değer |
|---|---|
| Dosya | `craftya-release.p12` (PKCS12) |
| Alias | `craftya` |
| Şifre | `Craftya2026Release` |
| Geçerlilik | 30 yıl (10950 gün) |
| Anahtar | RSA 2048 |

## Sertifika parmak izleri

**SHA-1** (Google Cloud Console → Android OAuth client için bu girilir):
```
05:07:A3:8E:EA:FF:4C:67:D3:94:09:85:89:46:9C:4D:65:3E:6D:15
```

**SHA-256** (App Links / asset links için):
```
FD:34:9A:A4:F3:87:07:35:88:4A:C6:7A:90:65:C9:DB:17:A7:24:E8:57:B1:36:C4:57:1F:74:D7:02:74:F2:53
```

## Kritik kurallar

1. **Bu dosya yedeğini al** — kaybedersen uygulamayı güncelleyemezsin (Android imza eşleşmez).
2. **Asla değiştirme.** İmza değişirse:
   - Google giriş kırılır (SHA-1 değişir)
   - Mevcut kurulumlar güncellenemez, oyuncular silip yeniden kurmak zorunda kalır
3. **Repo'da tutulur** (şu an öyle) — proje public olacaksa `.gitignore`'a alınmalı ve CI'da secret olarak verilmeli.
4. `debug` build için ayrı keystore gerekmez; debug imzası farklı SHA-1 üretir.
   Google Console'a **hem release hem debug SHA-1** eklenmeli, yoksa test ederken giriş çalışmaz.

## Google ile giriş kurulumu

1. Google Cloud Console → yeni proje "Craftya"
2. **OAuth consent screen** → External → uygulama adı "Craftya", kapsam: `email`, `profile`
3. **Credentials → Create → OAuth client ID → Android**
   - Package name: `com.craftya.game`
   - SHA-1: yukarıdaki
4. Aynı adımı **Web application** client için de yap (Supabase id_token doğrulaması bunu ister)
5. Supabase → Authentication → Providers → **Google** → Web client ID + secret gir
6. Client tarafı: `signInWithIdToken({ provider: 'google', token: <id_token> })`

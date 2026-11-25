-- ============================================================================
--  Craftya — global veri katmanı (Supabase / Postgres)
-- ============================================================================
--  NEDEN: 6 oda aynı gerçeği görmeli. Ban bir kez atılır her odada geçerli.
--  NE DEĞİL: dünya verisi burada DURMAZ (SQLite, oda diski). Blok geçmişi
--  CoreProtect'te kalır — Postgres'e yazmak 100k istek/gün limitini bitirir.
--
--  GÜVENLİK: RLS her tabloda açık. anon anahtar sadece "okuma" görür.
--  Yazma yalnızca Edge Function / service role ile. service_role anahtarı
--  asla istemciye/eklentiye gömülmez.
-- ============================================================================

-- ---------------------------------------------------------------- oyuncular
create table if not exists players (
  id            uuid primary key default gen_random_uuid(),
  username      text not null unique,          -- cracked: isim = kimlik
  username_lower text generated always as (lower(username)) stored,
  first_seen    timestamptz not null default now(),
  last_seen     timestamptz not null default now(),
  playtime_sec  bigint not null default 0,
  -- kimlik bağı: site (Discord) ↔ oyun
  discord_id    text unique,
  link_code     text,                          -- /link komutu için tek kullanımlık
  link_expires  timestamptz,
  linked_at     timestamptz
);
create index if not exists players_last_seen on players (last_seen desc);

-- ---------------------------------------------------------------- istatistik
create table if not exists player_stats (
  player_id     uuid primary key references players(id) on delete cascade,
  blocks_broken bigint not null default 0,
  blocks_placed bigint not null default 0,
  mobs_killed   bigint not null default 0,
  deaths        bigint not null default 0,
  bedwars_wins  int    not null default 0,
  bedwars_games int    not null default 0,
  duello_wins   int    not null default 0,
  parkur_best_ms int   not null default 0,
  updated_at    timestamptz not null default now()
);

-- ---------------------------------------------------------------- ekonomi (global tek para)
create table if not exists balances (
  player_id  uuid primary key references players(id) on delete cascade,
  amount     bigint not null default 0 check (amount >= 0),
  updated_at timestamptz not null default now()
);

-- ---------------------------------------------------------------- ban (tüm odalarda geçerli)
create table if not exists bans (
  id         bigserial primary key,
  username   text not null,
  ip         inet,
  reason     text not null default 'kural ihlali',
  banned_by  text not null,
  created_at timestamptz not null default now(),
  expires_at timestamptz,              -- null = kalıcı
  active     boolean not null default true
);
create index if not exists bans_active_name on bans (lower(username)) where active;

-- ---------------------------------------------------------------- kozmetik sahipliği
create table if not exists cosmetics_owned (
  player_id  uuid not null references players(id) on delete cascade,
  cosmetic   text not null,            -- 'cape_dragon', 'tag_kurucu'
  acquired_at timestamptz not null default now(),
  primary key (player_id, cosmetic)
);

-- ---------------------------------------------------------------- canlı oturum (site: kim online)
create table if not exists sessions (
  player_id  uuid primary key references players(id) on delete cascade,
  room       text not null,            -- lobby/survival/bedwars...
  joined_at  timestamptz not null default now(),
  seen_at    timestamptz not null default now()
);

-- ---------------------------------------------------------------- RLS
alter table players         enable row level security;
alter table player_stats    enable row level security;
alter table balances        enable row level security;
alter table bans            enable row level security;
alter table cosmetics_owned enable row level security;
alter table sessions        enable row level security;

-- Okuma: herkes (site liderlik tablosu gösterecek)
drop policy if exists read_players  on players;
drop policy if exists read_stats    on player_stats;
drop policy if exists read_balances on balances;
drop policy if exists read_bans     on bans;
drop policy if exists read_cosmetics on cosmetics_owned;
drop policy if exists read_sessions on sessions;

create policy read_players   on players         for select using (true);
create policy read_stats     on player_stats    for select using (true);
create policy read_balances  on balances        for select using (true);
create policy read_bans      on bans            for select using (true);
create policy read_cosmetics on cosmetics_owned for select using (true);
create policy read_sessions  on sessions        for select using (true);

-- Yazma: SADECE service role (Edge Function). anon hiçbir tabloya yazamaz.
-- (policy tanımlamadığımız için anon insert/update/delete yapamaz — bilinçli)

-- ---------------------------------------------------------------- view'lar
create or replace view leaderboard_blocks as
  select p.username, s.blocks_broken, p.last_seen
  from player_stats s join players p on p.id = s.player_id
  order by s.blocks_broken desc limit 50;

create or replace view online_now as
  select p.username, s.room, s.seen_at
  from sessions s join players p on p.id = s.player_id
  where s.seen_at > now() - interval '2 minutes'
  order by s.room, p.username;

create or replace view active_bans as
  select username, ip, reason, banned_by, created_at, expires_at
  from bans
  where active and (expires_at is null or expires_at > now())
  order by created_at desc;

-- ---------------------------------------------------------------- fonksiyon: oyuncu kaydı
-- Eklenti (bot) bu fonksiyonu çağırır; tek istekte oyuncu + istatistik + bakiye.
create or replace function register_player(p_username text)
returns uuid language plpgsql security definer as $$
declare pid uuid;
begin
  insert into players (username) values (p_username)
    on conflict (username) do update set last_seen = now()
    returning id into pid;
  insert into player_stats (player_id) values (pid) on conflict do nothing;
  insert into balances (player_id, amount) values (pid, 0) on conflict do nothing;
  return pid;
end $$;

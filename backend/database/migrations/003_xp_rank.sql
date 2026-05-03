-- Add XP column for gamification rank system
ALTER TABLE users ADD COLUMN xp INT NOT NULL DEFAULT 0;

-- Reset all existing users to Goblin Stadium (they have 0 XP)
UPDATE users SET trader_level = 'Goblin Stadium';

-- Update default for new registrations
ALTER TABLE users ALTER COLUMN trader_level SET DEFAULT 'Goblin Stadium';

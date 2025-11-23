// frontend/src/pages/Register.tsx

import { useState} from 'react';
import type { FormEvent } from 'react';
import { useNavigate, Link } from 'react-router-dom';
import { useAuth } from '../contexts/AuthContext';

export default function Register() {
  const navigate = useNavigate();
  const { register } = useAuth();

  // Form state
  const [username, setUsername] = useState('');
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [confirmPassword, setConfirmPassword] = useState('');
  const [error, setError] = useState('');
  const [isLoading, setIsLoading] = useState(false);

  // Handle form submission
  const handleSubmit = async (e: FormEvent) => {
    e.preventDefault();
    setError('');

    // Validate passwords match
    if (password !== confirmPassword) {
      setError('Passwords do not match');
      return;
    }

    // Validate password length
    if (password.length < 6) {
      setError('Password must be at least 6 characters');
      return;
    }

    setIsLoading(true);

    try {
      // Call register function from AuthContext
      await register({ username, email, password });
      
      // Redirect to dashboard on success
      navigate('/dashboard');
    } catch (err: any) {
      // Display error message
      const message = err.response?.data?.error || 'Registration failed. Please try again.';
      setError(message);
    } finally {
      setIsLoading(false);
    }
  };

  return (
    <div className="min-h-screen flex items-center justify-center bg-gradient-to-br from-clash-dark via-purple-900 to-clash-dark py-12">
      <div className="max-w-md w-full mx-4">
        {/* Card Container */}
        <div className="bg-slate-800/50 backdrop-blur-lg border border-purple-500/20 rounded-2xl shadow-2xl p-8">
          
          {/* Header */}
          <div className="text-center mb-8">
            <h1 className="text-4xl font-bold text-white mb-2">
              Create Account
            </h1>
            <p className="text-slate-400">
              Join the trading platform
            </p>
          </div>

          {/* Error Message */}
          {error && (
            <div className="mb-4 p-4 bg-red-500/10 border border-red-500/50 rounded-lg">
              <p className="text-red-400 text-sm">{error}</p>
            </div>
          )}

          {/* Register Form */}
          <form onSubmit={handleSubmit} className="space-y-5">
            
            {/* Username Field */}
            <div>
              <label htmlFor="username" className="block text-sm font-medium text-slate-300 mb-2">
                Username
              </label>
              <input
                id="username"
                type="text"
                value={username}
                onChange={(e) => setUsername(e.target.value)}
                required
                minLength={3}
                className="w-full px-4 py-3 bg-slate-900/50 border border-slate-700 rounded-lg 
                         text-white placeholder-slate-500 focus:outline-none focus:border-purple-500 
                         focus:ring-2 focus:ring-purple-500/20 transition"
                placeholder="Choose a username"
              />
            </div>

            {/* Email Field */}
            <div>
              <label htmlFor="email" className="block text-sm font-medium text-slate-300 mb-2">
                Email
              </label>
              <input
                id="email"
                type="email"
                value={email}
                onChange={(e) => setEmail(e.target.value)}
                required
                className="w-full px-4 py-3 bg-slate-900/50 border border-slate-700 rounded-lg 
                         text-white placeholder-slate-500 focus:outline-none focus:border-purple-500 
                         focus:ring-2 focus:ring-purple-500/20 transition"
                placeholder="your@email.com"
              />
            </div>

            {/* Password Field */}
            <div>
              <label htmlFor="password" className="block text-sm font-medium text-slate-300 mb-2">
                Password
              </label>
              <input
                id="password"
                type="password"
                value={password}
                onChange={(e) => setPassword(e.target.value)}
                required
                minLength={6}
                className="w-full px-4 py-3 bg-slate-900/50 border border-slate-700 rounded-lg 
                         text-white placeholder-slate-500 focus:outline-none focus:border-purple-500 
                         focus:ring-2 focus:ring-purple-500/20 transition"
                placeholder="At least 6 characters"
              />
            </div>

            {/* Confirm Password Field */}
            <div>
              <label htmlFor="confirmPassword" className="block text-sm font-medium text-slate-300 mb-2">
                Confirm Password
              </label>
              <input
                id="confirmPassword"
                type="password"
                value={confirmPassword}
                onChange={(e) => setConfirmPassword(e.target.value)}
                required
                className="w-full px-4 py-3 bg-slate-900/50 border border-slate-700 rounded-lg 
                         text-white placeholder-slate-500 focus:outline-none focus:border-purple-500 
                         focus:ring-2 focus:ring-purple-500/20 transition"
                placeholder="Confirm your password"
              />
            </div>

            {/* Submit Button */}
            <button
              type="submit"
              disabled={isLoading}
              className="w-full py-3 px-4 bg-gradient-to-r from-purple-600 to-blue-600 
                       text-white font-semibold rounded-lg shadow-lg hover:from-purple-700 
                       hover:to-blue-700 focus:outline-none focus:ring-2 focus:ring-purple-500 
                       focus:ring-offset-2 focus:ring-offset-slate-900 transition disabled:opacity-50 
                       disabled:cursor-not-allowed"
            >
              {isLoading ? 'Creating account...' : 'Create Account'}
            </button>
          </form>

          {/* Login Link */}
          <div className="mt-6 text-center">
            <p className="text-slate-400 text-sm">
              Already have an account?{' '}
              <Link 
                to="/login" 
                className="text-purple-400 hover:text-purple-300 font-semibold transition"
              >
                Sign in
              </Link>
            </p>
          </div>
        </div>

        {/* Info Text */}
        <p className="text-center text-slate-500 text-sm mt-6">
          Get 100,000 starting gold • No credit card required
        </p>
      </div>
    </div>
  );
}